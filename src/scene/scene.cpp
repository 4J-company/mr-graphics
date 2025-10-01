#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"

mr::Scene::Scene(RenderContext &render_context)
  : _parent(&render_context)
  , _camera_uniform_buffer(_parent->vulkan_state(), sizeof(ShaderCameraData))
  , _transforms(_parent->vulkan_state(), max_scene_instances * sizeof(mr::Matr4f))
  , _bounds(_parent->vulkan_state(),     max_scene_instances * sizeof(mr::AABBf))
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

  auto dir_light_handle = ResourceManager<DirectionalLight>::get().create(mr::unnamed, *this, direction, color);
  lights<DirectionalLight>().push_back(dir_light_handle);
  return dir_light_handle;
}

mr::ModelHandle mr::Scene::create_model(std::string_view filename) noexcept
{
  ASSERT(_parent != nullptr);

  auto model_handle = ResourceManager<Model>::get().create(mr::unnamed, *this, filename);

  _models.push_back(model_handle);
  return model_handle;
}

void mr::Scene::update(OptionalInputStateReference input_state_ref) noexcept
{
  ASSERT(_parent != nullptr);

  _transforms.write(std::span(_transforms_data));
  _bounds.write(std::span(_bounds_data));
  _visibility.write(std::span(_visibility_data));

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
