#include "base_light.hpp"

mr::Light::Light(const VulkanState &state, LightsRenderData &light_render_data, const Vec3f &color, size_t ubo_size)
  : _light_render_data(&light_render_data), _color(color), _uniform_buffer(state, ubo_size)
{
}