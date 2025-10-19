#include "base_light.hpp"
#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"

mr::Light::Light(Scene &scene, const Vec3f &color, size_t ubo_size)
  : _scene(&scene)
  , _lights_render_data(&_scene->render_context().lights_render_data())
  , _color(color)
  , _uniform_buffer(scene.render_context().vulkan_state(), ubo_size)
{
}
