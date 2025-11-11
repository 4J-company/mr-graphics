#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"
#include "manager/manager.hpp"

mr::Scene::Scene(RenderContext &render_context)
  : _parent(&render_context)
  , _camera_uniform_buffer(_parent->vulkan_state(), sizeof(ShaderCameraData))
  , _transfer_command_unit(_parent->vulkan_state())
  , _transforms(_parent->vulkan_state(), max_scene_instances * sizeof(mr::Matr4f))
  , _visibility(_parent->vulkan_state(), max_scene_instances * sizeof(uint32_t))
{
  ASSERT(_parent != nullptr);

  _camera.cam() = mr::math::Camera<float>({1}, {-1}, {0, 1, 0});
  _camera.cam().projection() = mr::math::Camera<float>::Projection(45_deg);

  _camera_buffer_id = render_context.bindless_set().register_resource(&_camera_uniform_buffer);
  _transforms_buffer_id = render_context.bindless_set().register_resource(&_transforms);
}

mr::Scene::~Scene()
{
  // TODO(dk6): Now this is segfault - RenderContext here has already destoryed - Scene class creating by RenderContext
  //            using Managers, RenderContext creating as std::shared_ptr by Application.
  //            So in end of main function RenderContext instance will be deleted, but Scene instances will be deleted
  //            only after main in end of c++ program when static resources destructors calls.
  //            For fix it RenderContext must store all Scene instances and in destructor delete it from Manager,
  //            but now Manager doesn't support it. Maybe we can add as tmp solution Scene
  //            method 'notify_render_context_deleted` and use it as destuctor and move Scene in "disabeld" state
  _parent->bindless_set().unregister_resource(&_camera_uniform_buffer);
  _parent->bindless_set().unregister_resource(&_transforms);
}

mr::DirectionalLightHandle mr::Scene::create_directional_light(const Norm3f &direction, const Vec3f &color) noexcept
{
  ASSERT(_parent != nullptr);

  _is_buffers_dirty = true;

  auto dir_light_handle = ResourceManager<DirectionalLight>::get().create(mr::unnamed, *this, direction, color);
  lights<DirectionalLight>().push_back(dir_light_handle);
  return dir_light_handle;
}

mr::ModelHandle mr::Scene::create_model(std::fs::path filename) noexcept
{
  ASSERT(_parent != nullptr);

  _is_buffers_dirty = true;

  auto model_handle = ResourceManager<Model>::get().create(mr::unnamed, *this, filename);

  _models.push_back(model_handle);
  for (const auto &[material, mesh] : model_handle->draws()) {
    auto [draw_it, is_new_pipeline] = _draws.insert({material->pipeline(), {}});
    auto &draw = draw_it->second;
    if (is_new_pipeline) {
      // TODO(dk6): I think max_scene_instances is too big number here
      draw.commands_buffer = StorageBuffer(_parent->vulkan_state(),
                                           sizeof(MeshFillDrawCommandInfo) * max_scene_instances,
                                           vk::BufferUsageFlagBits::eStorageBuffer |
                                           vk::BufferUsageFlagBits::eIndirectBuffer |
                                           vk::BufferUsageFlagBits::eTransferDst);
      draw.draws_commands = StorageBuffer(_parent->vulkan_state(),
                                          sizeof(vk::DrawIndexedIndirectCommand) * max_scene_instances,
                                          vk::BufferUsageFlagBits::eStorageBuffer |
                                          vk::BufferUsageFlagBits::eIndirectBuffer);
      draw.draws_count_buffer = StorageBuffer(_parent->vulkan_state(),
                                              sizeof(uint32_t),
                                              vk::BufferUsageFlagBits::eStorageBuffer |
                                              vk::BufferUsageFlagBits::eIndirectBuffer |
                                              // for vkCmdFill
                                              vk::BufferUsageFlagBits::eTransferDst);

      draw.commands_buffer_id = _parent->bindless_set().register_resource(&draw.commands_buffer);
      draw.draws_commands_buffer_id = _parent->bindless_set().register_resource(&draw.draws_commands);
      draw.draws_count_buffer_id = _parent->bindless_set().register_resource(&draw.draws_count_buffer);

      draw.meshes_render_info = StorageBuffer(_parent->vulkan_state(), sizeof(Mesh::RenderInfo) * max_scene_instances);
      draw.meshes_render_info_id = _parent->bindless_set().register_resource(&draw.meshes_render_info);
    }

    draw.meshes.emplace_back(&mesh);
    static_assert(sizeof(AABBf::VecT) == 4 * 4);
    draw.commands_buffer_data.emplace_back(MeshFillDrawCommandInfo {
      .draw_command = vk::DrawIndexedIndirectCommand {
        .indexCount = mesh.element_count(),
        .instanceCount = mesh.num_of_instances(),
        .firstIndex = static_cast<uint32_t>(mesh._ibufs[0].offset / sizeof(uint32_t)),
        .vertexOffset = static_cast<int32_t>(mesh._vbufs[0].offset / position_bytes_size),
        .firstInstance = 0,
      },
      .bound_box = mesh._bound_box,
    });
    draw.meshes_render_info_data.emplace_back(Mesh::RenderInfo {
      .mesh_offset = mesh._mesh_offset,
      .instance_offset = mesh._instance_offset,
      .material_ubo_id = material->material_ubo_id(),
      .camera_buffer_id = _camera_buffer_id,
      .transforms_buffer_id = _transforms_buffer_id,
    });

    _triangles_number += mesh.element_count() / 3;
    _vertexes_number += mesh._vbufs[0].vertex_count;
  }

  return model_handle;
}

void mr::Scene::update(OptionalInputStateReference input_state_ref) noexcept
{
  ASSERT(_parent != nullptr);

  vk::UniqueFence fence {};
  FenceGuard guard { _parent->vulkan_state().device() };
  if (_is_buffers_dirty) {
    _transfer_command_unit.begin();
    _transforms.write(_transfer_command_unit, std::span(_transforms_data));
    // _bounds.write(_transfer_command_unit, std::span(_bounds_data));
    _visibility.write(_transfer_command_unit, std::span(_visibility_data));
    _transfer_command_unit.end();

    fence = _transfer_command_unit.submit(_parent->vulkan_state());

    _is_buffers_dirty = false;
    guard.fence = fence.get();
  }

  if (input_state_ref) {
    const auto &input_state = input_state_ref->get();

    mr::Vec3f angular_delta {
      input_state.mouse_pos_delta().x() / _parent->extent().width,
      -input_state.mouse_pos_delta().y() / _parent->extent().height, // "-" to adjust for screen-space y coordinate being inverted
      0
    };

    _camera.turn(angular_delta);

    // camera controls
    if (input_state.key_pressed(vkfw::Key::eW)) {
      _camera.move(_camera.cam().direction());
    }
    if (input_state.key_pressed(vkfw::Key::eA)) {
      _camera.move(-_camera.cam().right());
    }
    if (input_state.key_pressed(vkfw::Key::eS)) {
      _camera.move(-_camera.cam().direction());
    }
    if (input_state.key_pressed(vkfw::Key::eD)) {
      _camera.move(_camera.cam().right());
    }
    if (input_state.key_pressed(vkfw::Key::eSpace)) {
      _camera.move(_camera.cam().up());
    }
    if (input_state.key_pressed(vkfw::Key::eLeftShift)) {
      _camera.move(_camera.cam().up());
    }
    if (input_state.key_tapped(vkfw::Key::e1)) {
      _camera.cam() = mr::math::Camera<float>({1}, {-1}, {0, 1, 0});
      _camera.cam().projection() = mr::math::Camera<float>::Projection(45_deg);
    }
    if (input_state.key_tapped(vkfw::Key::e2)) {
      _camera.cam() = mr::math::Camera<float>({10}, {-1}, {0, 1, 0});
      _camera.cam().projection() = mr::math::Camera<float>::Projection(45_deg);
    }
    if (input_state.key_tapped(vkfw::Key::e3)) {
      _camera.cam() = mr::math::Camera<float>({100}, {-1}, {0, 1, 0});
      _camera.cam().projection() = mr::math::Camera<float>::Projection(45_deg);
    }
  }

  update_camera_buffer();
}

void mr::Scene::update_camera_buffer() noexcept
{
  mr::ShaderCameraData cam_data {
    .vp = _camera.viewproj(),
    .campos = _camera.cam().position(),
    .fov = static_cast<float>(_camera.fov()),
    .gamma = _camera.gamma(),
    .speed = _camera.speed(),
    .sens = _camera.sensetivity(),
  };

  _camera_uniform_buffer.write(std::span<mr::ShaderCameraData> {&cam_data, 1});
}
