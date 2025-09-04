#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"

mr::Scene::Scene(const RenderContext &render_context)
  : _parent(&render_context)
  , _camera_uniform_buffer(_parent->vulkan_state(), sizeof(ShaderCameraData))
{
  ASSERT(_parent != nullptr);
}

mr::DirectionalLightHandle mr::Scene::create_directional_light(
  const Norm3f &direction, const Vec3f &color) noexcept
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

void mr::Scene::update(const InputState &input_state) noexcept
{
  ASSERT(_parent != nullptr);

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
    _camera.move(-_camera.cam().up());
  }
  if (input_state.key_pressed(vkfw::Key::eLeftShift)) {
    _camera.move(_camera.cam().up());
  }
}