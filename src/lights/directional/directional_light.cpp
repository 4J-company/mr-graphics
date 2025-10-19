#include "lights/directional/directional_light.hpp"
#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"

mr::graphics::DirectionalLight::DirectionalLight(Scene &scene, const Norm3f &direction, const Vec3f &color)
  : Light(scene, color, sizeof(ShaderUniformBuffer))
  , _direction(direction)
{
  _uniform_buffer_id = scene.render_context().bindless_set().register_resource(&_uniform_buffer);
}

mr::graphics::DirectionalLight::~DirectionalLight() noexcept
{
  _scene->render_context().bindless_set().unregister_resource(&_uniform_buffer);
}

void mr::graphics::DirectionalLight::shade(CommandUnit &unit) const noexcept
{
  if (not _enabled) {
    return;
  }
  _update_ubo();

  uint32_t push_data[] {
    _scene->camera_buffer_id(),
    _uniform_buffer_id,
  };
  unit->pushConstants(pipeline().layout(), vk::ShaderStageFlagBits::eAllGraphics,
                      0, sizeof(push_data), push_data);

  // ---------------------------------------------------------------------------------
  // TODO(dk6): all code above can be called once for all directional lights per frame
  // ---------------------------------------------------------------------------------

  // TODO(dk6): use pipeline.apply()
  unit->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline().pipeline());
  // TODO(dk6): use set.bind()
  unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                           pipeline().layout(),
                           0, {lights_descriptor_set()}, {});

  unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                           pipeline().layout(),
                           1, {_scene->render_context().bindless_set()}, {});

  // TODO(dk6): use instansing here
  unit->drawIndexed(index_buffer().element_count(), 1, 0, 0, 0);
}

void mr::graphics::DirectionalLight::_update_ubo() const noexcept
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
