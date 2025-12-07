#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"
#include "manager/manager.hpp"

mr::Scene::Scene(RenderContext &render_context)
  : _parent(&render_context)
  , _camera_uniform_buffer(_parent->vulkan_state(), sizeof(ShaderCameraData))
  , _transfer_command_unit(_parent->vulkan_state())
  , _transfers_semaphore(_parent->vulkan_state().device().createSemaphoreUnique({}).value)
  , _transforms(_parent->vulkan_state(), max_scene_instances * sizeof(mr::Matr4f))
  , _bound_boxes(_parent->vulkan_state(), max_scene_instances * sizeof(AABBf))
  , _visible_instances_transforms(_parent->vulkan_state(), max_scene_instances * sizeof(mr::Matr4f))
  , _visibility(_parent->vulkan_state(), max_scene_instances * sizeof(uint32_t))
  , _counters_buffer(_parent->vulkan_state(), max_scene_instances * sizeof(uint32_t),
                     vk::BufferUsageFlagBits::eStorageBuffer |
                     vk::BufferUsageFlagBits::eIndirectBuffer |
                     vk::BufferUsageFlagBits::eTransferSrc)
{
  ASSERT(_parent != nullptr);

  _camera.cam() = mr::math::Camera<float>({1}, {-1}, {0, 1, 0});
  _camera.cam().projection() = mr::math::Camera<float>::Projection(45_deg);

  _camera_buffer_id = render_context.bindless_set().register_resource(&_camera_uniform_buffer);
  _transforms_buffer_id = render_context.bindless_set().register_resource(&_transforms);
  _render_transforms_buffer_id = render_context.bindless_set().register_resource(&_visible_instances_transforms);
  _bound_boxes_buffer_id = render_context.bindless_set().register_resource(&_bound_boxes);
  _counters_buffer_id = render_context.bindless_set().register_resource(&_counters_buffer);
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
  _parent->bindless_set().unregister_resource(&_visible_instances_transforms);
  _parent->bindless_set().unregister_resource(&_counters_buffer);
  _parent->bindless_set().unregister_resource(&_bound_boxes);
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
      // TODO(dk6): I think max_scene_instances is too big number here. But anyway we must use dynamic buffers
      draw.instances_data_buffer = StorageBuffer(_parent->vulkan_state(),
                                                sizeof(MeshInstanceCullingData) * max_scene_instances,
                                                vk::BufferUsageFlagBits::eStorageBuffer |
                                                vk::BufferUsageFlagBits::eTransferDst);
      draw.meshes_data_buffer = StorageBuffer(_parent->vulkan_state(),
                                           sizeof(MeshCullingData) * max_scene_instances,
                                           vk::BufferUsageFlagBits::eStorageBuffer |
                                           vk::BufferUsageFlagBits::eTransferDst);
      draw.draw_commands_buffer = StorageBuffer(_parent->vulkan_state(),
                                                sizeof(vk::DrawIndexedIndirectCommand) * max_scene_instances,
                                                vk::BufferUsageFlagBits::eStorageBuffer |
                                                vk::BufferUsageFlagBits::eIndirectBuffer);

      draw.draw_counter_index = _current_counter_index++;

      draw.instances_data_buffer_id = _parent->bindless_set().register_resource(&draw.instances_data_buffer);
      draw.meshes_data_buffer_id = _parent->bindless_set().register_resource(&draw.meshes_data_buffer);
      draw.draw_commands_buffer_id = _parent->bindless_set().register_resource(&draw.draw_commands_buffer);

      draw.meshes_render_info = StorageBuffer(_parent->vulkan_state(), sizeof(Mesh::RenderInfo) * max_scene_instances);
      draw.meshes_render_info_id = _parent->bindless_set().register_resource(&draw.meshes_render_info);
    }
    draw.meshes.emplace_back(&mesh);

    // TODO: here can be trouble in multithreading
    uint32_t bound_box_index = static_cast<uint32_t>(_bound_boxes_data.size());
    _bound_boxes_data.emplace_back(mesh._bound_box);
    for (uint32_t i = 0; i < mesh._instance_count; i++) {
      _parent->draw_bound_box(_transforms_buffer_id, mesh._instance_offset + i,
                              _bound_boxes_buffer_id, bound_box_index);
    }

    uint32_t mesh_culling_data_index = static_cast<uint32_t>(draw.meshes_data_buffer_data.size());
    draw.meshes_data_buffer_data.emplace_back(MeshCullingData {
      .draw_command = vk::DrawIndexedIndirectCommand {
        .indexCount = mesh.element_count(),
        .instanceCount = mesh.num_of_instances(),
        .firstIndex = static_cast<uint32_t>(mesh._ibufs[0].offset / sizeof(uint32_t)),
        .vertexOffset = static_cast<int32_t>(mesh._vbufs[0].offset / position_bytes_size),
        .firstInstance = 0,
      },
      .render_info = Mesh::RenderInfo {
        .mesh_offset = mesh._mesh_offset,
        .instance_offset = mesh._instance_offset,
        .material_ubo_id = material->material_ubo_id(),
      },
      .instance_counter_index = _current_counter_index++,
      .bound_box_index = bound_box_index,
    });

    for (uint32_t instance = 0; instance < mesh._instance_count; instance++) {
      draw.instances_data_buffer_data.emplace_back(MeshInstanceCullingData {
        .transform_index = mesh._instance_offset + instance,
        .visible_last_frame = 1, // all meshes visible firstly
        .mesh_culling_data_index = mesh_culling_data_index,
      });
    }

    _triangles_number += mesh.element_count() / 3;
    _vertexes_number += mesh._vbufs[0].vertex_count;
  }

  return model_handle;
}

void mr::Scene::update(OptionalInputStateReference input_state_ref) noexcept
{
  ASSERT(_parent != nullptr);

  _was_transfer_in_this_frame = false;
  if (_is_buffers_dirty) {
    if (_transfers_fence) {
      _parent->vulkan_state().device().waitForFences({_transfers_fence.get()}, vk::True, UINT64_MAX);
    }
    _transfer_command_unit.begin();

    _transforms.write(_transfer_command_unit, std::span(_transforms_data));
    _bound_boxes.write(_transfer_command_unit, std::span(_bound_boxes_data));
    _visibility.write(_transfer_command_unit, std::span(_visibility_data));

    for (auto &[_, draw] : _draws) {
      draw.meshes_data_buffer.write(_transfer_command_unit, std::span(draw.meshes_data_buffer_data));
      draw.instances_data_buffer.write(_transfer_command_unit, std::span(draw.instances_data_buffer_data));
    }

    _transfer_command_unit.end();

    _transfer_command_unit.add_signal_semaphore(_transfers_semaphore.get());

    // Don't use fence becase sync is provided by semaphores
    _transfers_fence = _transfer_command_unit.submit(_parent->vulkan_state());

    _is_buffers_dirty = false;
    _was_transfer_in_this_frame = true;
  }

  if (input_state_ref) {
    const auto &input_state = input_state_ref->get();

    float min_speed = 0.05;
    float max_speed = 5;
    float speed_delta_coef = 0.05;
    float new_speed = _camera.speed() + input_state.mouse_scroll() * speed_delta_coef;
    if (new_speed >= min_speed && new_speed <= max_speed) {
      _camera.speed(new_speed);
    }

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
    if (input_state.key_tapped(vkfw::Key::eB)) {
      _draw_bound_rects = !_draw_bound_rects;
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
    .frustum_planes = _camera.frustum_planes(),
  };

  _camera_uniform_buffer.write(std::span<mr::ShaderCameraData> {&cam_data, 1});
}
