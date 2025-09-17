#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"

mr::Scene::Scene(const RenderContext &render_context)
  : _parent(&render_context)
  , _camera_uniform_buffer(_parent->vulkan_state(), sizeof(ShaderCameraData))
  , _transforms(_parent->vulkan_state(), 1024 * sizeof(mr::Matr4f))
  , _bounds(_parent->vulkan_state(),     1024 * sizeof(mr::AABBf))
  , _visibility(_parent->vulkan_state(), 1024 * sizeof(uint32_t))
{
  ASSERT(_parent != nullptr);
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

  if (input_state_ref) {
    const auto &input_state = input_state_ref->get();

    _transforms.write(std::span(_transforms_data));
    _bounds.write(std::span(_bounds_data));
    _visibility.write(std::span(_visibility_data));

    _camera.turn({input_state.mouse_pos_delta().x() / _parent->extent().width,
                  input_state.mouse_pos_delta().y() / _parent->extent().height,
                  0});

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
      // TODO(mt6): remove here -
      _camera.move(-_camera.cam().up());
    }
    if (input_state.key_pressed(vkfw::Key::eLeftShift)) {
      _camera.move(_camera.cam().up());
    }
  }

  _update_camera_buffer();
}

void mr::Scene::_update_camera_buffer() noexcept
{
  _camera.cam() = mr::math::Camera<float>({1}, {-1}, {0, 1, 0});
  _camera.cam().projection() = mr::math::Camera<float>::Projection(45_deg);

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
