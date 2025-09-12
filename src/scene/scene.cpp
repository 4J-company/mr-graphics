#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"

// TODO(dk6): this is temporary changes, while mr::Camera works incorrect
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

mr::Scene::Scene(const RenderContext &render_context)
  : _parent(&render_context)
  , _camera_uniform_buffer(_parent->vulkan_state(), sizeof(ShaderCameraData))
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
    // TODO(mt6): remove here -
    _camera.move(-_camera.cam().up());
  }
  if (input_state.key_pressed(vkfw::Key::eLeftShift)) {
    _camera.move(_camera.cam().up());
  }

  _update_camera_buffer();
}

void mr::Scene::_update_camera_buffer() noexcept
{
  // cam.cam() = mr::Camera<float>({1}, {-1}, {0, 1, 0});
  // cam.cam().projection() = mr::Camera<float>::Projection(0.25_pi);

  // TODO(dk6): this is temporary changes, while mr::Camera works incorrect
  glm::mat4 view = glm::lookAt(glm::vec3{1.f, 1.f, 1.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  glm::mat4 proj = glm::perspective(glm::radians(45.f), 1920.f / 1080.f, 0.1f, 100.f);
  glm::mat4 vp = proj * view;

  mr::Matr4f mrvp {
    vp[0][0], vp[0][1], vp[0][2], vp[0][3],
    vp[1][0], vp[1][1], vp[1][2], vp[1][3],
    vp[2][0], vp[2][1], vp[2][2], vp[2][3],
    vp[3][0], vp[3][1], vp[3][2], vp[3][3],
  };

  mr::ShaderCameraData cam_data {
    // .vp = cam.viewproj(),
    .vp = mrvp,
    .campos = _camera.cam().position(),
    .fov = static_cast<float>(_camera.fov()),
    .gamma = _camera.gamma(),
    .speed = _camera.speed(),
    .sens = _camera.sensetivity(),
  };

  _camera_uniform_buffer.write(std::span<mr::ShaderCameraData> {&cam_data, 1});

}