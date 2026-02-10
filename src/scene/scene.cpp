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
  , _bound_spheres(_parent->vulkan_state(), max_scene_instances * sizeof(AABBf))
  , _visibility(_parent->vulkan_state(), max_scene_instances * sizeof(uint32_t))
  , _counters_buffer(_parent->vulkan_state(), max_scene_instances * sizeof(uint32_t),
                     vk::BufferUsageFlagBits::eStorageBuffer |
                     vk::BufferUsageFlagBits::eIndirectBuffer |
                     vk::BufferUsageFlagBits::eTransferSrc)
{
  ASSERT(_parent != nullptr);

  _camera.cam() = mr::math::Camera<float>(Vec3f{1.f}, Vec3f{0});
  _camera.cam().projection() = mr::math::Camera<float>::Projection(45_deg, 0.01, 1000.0);

  _camera_buffer_id = render_context.bindless_set().register_resource(&_camera_uniform_buffer);
  _transforms_buffer_id = render_context.bindless_set().register_resource(&_transforms);
  _bound_boxes_buffer_id = render_context.bindless_set().register_resource(&_bound_boxes);
  _bound_spheres_buffer_id = render_context.bindless_set().register_resource(&_bound_spheres);
  _counters_buffer_id = render_context.bindless_set().register_resource(&_counters_buffer);

  if (is_render_option_enabled(_parent->options(), RenderOptions::EnableCullingVisualiztion)) {
    // TODO(dk6): Try change uint int to byte && use dynamic buffer
    _occluded_instances_state_buffer = StorageBuffer(_parent->vulkan_state(),
                                                     sizeof(uint32_t) * max_scene_instances);
    _occluded_instances_state_buffer_id =
      _parent->bindless_set().register_resource(&_occluded_instances_state_buffer);

    // fill by 1
    std::vector<uint32_t> data(max_scene_instances, 0b10001);
    CommandUnit cmd_unit(_parent->vulkan_state());
    cmd_unit.begin();
    _occluded_instances_state_buffer.write(cmd_unit, std::span(data));
    cmd_unit.end();
    UniqueFenceGuard(
      _parent->vulkan_state().device(),
      cmd_unit.submit(_parent->vulkan_state())
    );
  }
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
  for (const auto &[material, model_mesh] : model_handle->draws()) {
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
      draw.draw_visibility_buffer = StorageBuffer(_parent->vulkan_state(),
                                                  sizeof(uint32_t) * max_scene_instances,
                                                  vk::BufferUsageFlagBits::eStorageBuffer);
      draw.draw_prefix_buffer = StorageBuffer(_parent->vulkan_state(),
                                              sizeof(uint32_t) * max_scene_instances,
                                              vk::BufferUsageFlagBits::eStorageBuffer);
      for (uint32_t i = 0; i < draw.scan_aux_buffers.size(); i++) {
        draw.scan_aux_buffers[i] = StorageBuffer(_parent->vulkan_state(),
                                                 sizeof(uint32_t) * max_scene_instances,
                                                 vk::BufferUsageFlagBits::eStorageBuffer);
      }

      draw.draw_counter_index = _current_counter_index++;

      draw.instances_data_buffer_id = _parent->bindless_set().register_resource(&draw.instances_data_buffer);
      draw.meshes_data_buffer_id = _parent->bindless_set().register_resource(&draw.meshes_data_buffer);
      draw.draw_commands_buffer_id = _parent->bindless_set().register_resource(&draw.draw_commands_buffer);
      draw.draw_visibility_buffer_id = _parent->bindless_set().register_resource(&draw.draw_visibility_buffer);
      draw.draw_prefix_buffer_id = _parent->bindless_set().register_resource(&draw.draw_prefix_buffer);
      for (uint32_t i = 0; i < draw.scan_aux_buffers.size(); i++) {
        draw.scan_aux_buffer_ids[i] = _parent->bindless_set().register_resource(&draw.scan_aux_buffers[i]);
      }

      draw.meshes_render_info = StorageBuffer(_parent->vulkan_state(), sizeof(Mesh::RenderInfo) * max_scene_instances);
      draw.meshes_render_info_id = _parent->bindless_set().register_resource(&draw.meshes_render_info);
    }
    const auto &mesh = model_mesh.mesh;
    draw.meshes.emplace_back(&mesh);

    uint32_t bound_box_index = static_cast<uint32_t>(_bound_boxes_data.size());
    _bound_boxes_data.emplace_back(mesh._bound_box);
    model_mesh.mesh_bound_box_id = bound_box_index;

    // TODO(dk6): use same index can be incorrect in multithread code - but anyway this is temporary solution,
    // I think we must merge these buffers
    _bound_spheres_data.emplace_back(mesh._bound_sphere);

    uint32_t mesh_culling_data_index = static_cast<uint32_t>(draw.meshes_data_buffer_data.size());
    model_mesh.mesh_scene_id = mesh_culling_data_index;
    uint32_t lod_index = 0;
    draw.meshes_data_buffer_data.emplace_back(MeshCullingData {
      .draw_command = vk::DrawIndexedIndirectCommand {
        .indexCount = mesh._ibufs[lod_index].elements_count,
        .instanceCount = 0,
        .firstIndex = static_cast<uint32_t>(mesh._ibufs[lod_index].offset / sizeof(uint32_t)),
        .vertexOffset = static_cast<int32_t>(mesh._vbufs[0].offset / position_bytes_size),
        .firstInstance = 0,
      },
      .render_info = Mesh::RenderInfo {
        .mesh_offset = mesh._mesh_offset,
        .instance_offset = mesh._instance_offset,
        .material_ubo_id = material->material_ubo_id(),
        .intances_render_info_buffer_id = model_mesh.intances_render_info_buffer_id,
      },
      .instance_counter_index = _current_counter_index++,
      .bound_box_index = bound_box_index,
    });
  }

  add_model_instance(model_handle, Matr4f::identity());

  return model_handle;
}

uint32_t mr::Scene::add_model_instance(ModelHandle model, Matr4f transform) noexcept {
  uint32_t index = model->_transforms_data.size();
  model->_transforms_data.emplace_back(transform);
  // Not it will works incorrect in multithreading
  uint32_t offset_in_transforms = _transforms_data.size();
  model->_offsets_of_instances.emplace_back(offset_in_transforms);

  for (const auto &[material, model_mesh] : model->draws()) {
    const auto &mesh = model_mesh.mesh;
    auto &draw = _draws[material->pipeline()];
    for (uint32_t instance = 0; instance < model_mesh.instances_number; instance++) {
      uint32_t instance_id = _transforms_data.size();
      _transforms_data.emplace_back(transform * model_mesh.transforms[instance]);

      draw.instances_data_buffer_data.emplace_back(MeshInstanceCullingData {
        .transform_index = instance_id,
        .visible_last_frame = 0b1,
        .mesh_culling_data_index = model_mesh.mesh_scene_id,
      });

      _parent->draw_bound_box(_transforms_buffer_id, instance_id,
                              _bound_boxes_buffer_id, _bound_spheres_buffer_id,
                              model_mesh.mesh_bound_box_id);
    }

    draw.meshes_data_buffer_data[model_mesh.mesh_scene_id].draw_command.instanceCount += model_mesh.transforms.size();

    _triangles_number += (mesh.element_count() / 3) * model_mesh.instances_number;
    _vertexes_number += (mesh._vbufs[0].vertex_count) * model_mesh.instances_number;
  }
  _is_buffers_dirty = true;

  return index;
}

void mr::Scene::update_model_transform(ModelHandle model,
                                       Matr4f transform, uint32_t instance) noexcept {
  uint32_t offset_in_transforms = model->_offsets_of_instances[instance];
  model->_transforms_data[instance] = transform;

  for (const auto &[material, model_mesh] : model->draws()) {
    const auto &mesh = model_mesh.mesh;
    auto &draw = _draws[material->pipeline()];
    for (uint32_t instance = 0; instance < model_mesh.instances_number; instance++) {
      _transforms_data[offset_in_transforms++] = transform * model_mesh.transforms[instance];
    }
  }
  _is_buffers_dirty = true;
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
    _bound_spheres.write(_transfer_command_unit, std::span(_bound_spheres_data));
    // _visibility.write(_transfer_command_unit, std::span(_visibility_data));

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

    float min_speed = 0.005;
    float max_speed = 20;
    float speed_delta_coef = 0.005;
    float new_speed = _camera.speed() + input_state.mouse_scroll() * speed_delta_coef;
    if (new_speed >= min_speed && new_speed <= max_speed) {
      _camera.speed(new_speed);
    }

    Vec3f angular_delta {
      input_state.mouse_pos_delta().x() / _parent->extent().width,
      -input_state.mouse_pos_delta().y() / _parent->extent().height, // "-" to adjust for screen-space y coordinate being inverted
      0
    };

    // Filter tiny cursor jitter from OS/window system to keep camera stable when idle.
    constexpr float mouse_jitter_deadzone = 1e-4f;
    if (std::abs(angular_delta.x()) > mouse_jitter_deadzone ||
        std::abs(angular_delta.y()) > mouse_jitter_deadzone) {
      _camera.turn(angular_delta);
    }

    // camera controls
    float speedup = input_state.key_pressed(vkfw::Key::eLeftShift) ? 10.0 : 1.0;
    if (input_state.key_pressed(vkfw::Key::eW)) {
      _camera.move(Vec3f(_camera.cam().direction()) * speedup);
    }
    if (input_state.key_pressed(vkfw::Key::eA)) {
      _camera.move(Vec3f(-_camera.cam().right()) * speedup);
    }
    if (input_state.key_pressed(vkfw::Key::eS)) {
      float speedup = input_state.key_pressed(vkfw::Key::eLeftShift) ? 10.0 : 1.0;
      _camera.move(Vec3f(-_camera.cam().direction()) * speedup);
    }
    if (input_state.key_pressed(vkfw::Key::eD)) {
      _camera.move(Vec3f(_camera.cam().right()) * speedup);
    }
    if (input_state.key_pressed(vkfw::Key::eSpace)) {
      _camera.move(Vec3f(_camera.cam().up()) * speedup);
    }
    if (input_state.key_pressed(vkfw::Key::eZ)) {
      _camera.move(Vec3f(-_camera.cam().up()) * speedup);
    }
    if (input_state.key_tapped(vkfw::Key::eP)) {
      std::println("({}, {}, {})", _camera.cam().position(), Vec3f(_camera.cam().direction()), Vec3f(_camera.cam().up()));
    }
    if (is_render_option_enabled(_parent->options(), RenderOptions::EnableCullingVisualiztion)) {
      if (input_state.key_tapped(vkfw::Key::eO)) {
        if (input_state.key_pressed(vkfw::Key::eLeftShift)) {
          _parent->clear_visibility();
        } else if (input_state.key_pressed(vkfw::Key::eLeftControl)) {
          _camera.cam() = _save_camera_on_visibility_save.cam();
        } else {
          _save_camera_on_visibility_save.cam() = _camera.cam();
          _parent->save_visibility();
        }
      }
    }

    if (input_state.key_tapped(vkfw::Key::e1)) {
      _camera.cam().set(Vec3f{1.f}, Vec3f{0});
    }
    if (input_state.key_tapped(vkfw::Key::e2)) {
      _camera.cam().set(Vec3f{10.f}, Vec3f{0});
    }
    if (input_state.key_tapped(vkfw::Key::e3)) {
      _camera.cam().set(Vec3f{100.f}, Vec3f{0});
    }
    if (input_state.key_tapped(vkfw::Key::e4)) {
      _camera.cam().set(Vec3f{500.f}, Vec3f{0});
    }
    if (input_state.key_tapped(vkfw::Key::e5)) {
      _camera.cam().set(Vec3f{10000.f}, Vec3f{0});
    }
    if (input_state.key_tapped(vkfw::Key::e6)) {
      _camera.cam().set(Vec3f{100000.f}, Vec3f{0});
    }
    if (input_state.key_tapped(vkfw::Key::e0)) {
      auto cam_pos = _camera.cam().position();
      _camera.cam().set(cam_pos, Vec3f{0});
    }

    if (input_state.key_tapped(vkfw::Key::eB)) {
      auto state = _parent->render_bounds_state();
      uint32_t s = (enum_cast(state) + 1) % RenderContext::RenderBoundsState::StatesNumber;
      _parent->render_bounds_state(enum_cast<RenderContext::RenderBoundsState>(s));
    }

    if (is_render_option_enabled(_parent->options(), RenderOptions::CollectPosInstanceId)) {
      if (input_state.mouse_button_tapped(vkfw::MouseButton::eLeft)) {
        auto pos = input_state.mouse_pos();
        uint32_t x = static_cast<uint32_t>(pos.x());
        uint32_t y = static_cast<uint32_t>(pos.y());
        auto pixel_opt = _parent->get_position_id_pixel(x, y);
        if (pixel_opt) {
          auto pixel = *pixel_opt;
          auto pos = Vec3f(pixel.x(), pixel.y(), pixel.z());
          uint32_t id = std::bit_cast<uint32_t>(pixel.w());
          std::println("pos: {}, id: {}", pos, id);
        }
      }
    }
  }

  update_camera_buffer();
}

void mr::Scene::update_camera_buffer() noexcept
{
  auto dir = _camera.cam().direction();
  mr::ShaderCameraData cam_data {
    .vp = _camera.viewproj(),
    .view = _camera.cam().perspective(),
    .proj = _camera.cam().frustum(),
    .near = _camera.cam().projection().distance,
    .far = _camera.cam().projection().far,
    .campos = _camera.cam().position(),
    .dir = mr::Vec4f(dir.x(), dir.y(), dir.z(), 0),
    .fov = static_cast<float>(_camera.fov()),
    .gamma = _camera.gamma(),
    .speed = _camera.speed(),
    .sens = _camera.sensetivity(),
    .frustum_planes = _camera.frustum_planes(),
  };

  _camera_uniform_buffer.write(std::span<mr::ShaderCameraData> {&cam_data, 1});
}
