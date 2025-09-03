#include "lights/directional/directional_light.hpp"

mr::DirectionalLight::DirectionalLight(const VulkanState &state, LightsRenderData &light_render_data,
                                       Norm3f direction, const Vec3f &color)
  : Light(state, light_render_data, color, sizeof(ShaderUniformBuffer))
  , _direction(direction)
{
  auto set1_optional = descriptor_allocator().allocate_set(set_layout());
  ASSERT(set1_optional.has_value());
  _set1 = std::move(set1_optional.value());

  std::array light_shader_resources {
    Shader::ResourceView(1, 0, &_uniform_buffer)
  };
  _set1.update(state, light_shader_resources);
}

void mr::DirectionalLight::shade(CommandUnit &unit) const noexcept
{
  // TODO(dk6): use pipeline.apply()
  unit->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline().pipeline());
  // TODO(dk6): use set.bind()
  unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                           pipeline().layout(),
                           0, {set0()}, {});

  // ---------------------------------------------------------------------------------
  // TODO(dk6): all code above can be called once for all directional lights per frame
  // ---------------------------------------------------------------------------------

  _update_ubo();
  unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                           pipeline().layout(),
                           1, {_set1}, {});

  unit->drawIndexed(index_buffer().element_count(), 1, 0, 0, 0);
}

void mr::DirectionalLight::_update_ubo() const noexcept
{
  if (!_updated) {
    return;
  }
  _updated = false;

  ShaderUniformBuffer ubo_data {
    // .direction = Vec4f(_direction, 1.0),
    // .color = Vec4f(_color, 1.0),
    .direction = Vec4f(_direction.x(), _direction.y(), _direction.z(), 1.0),
    .color = Vec4f(_color.x(), _color.y(), _color.z(), 1.0),
  };
  _uniform_buffer.write(std::span<ShaderUniformBuffer> {&ubo_data, 1});
}