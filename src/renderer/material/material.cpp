#include "material/material.hpp"

mr::Material::Material(const VulkanState &state,
                       const RenderContext &render_context,
                       mr::ShaderHandle shader,
                       std::span<float> ubo_data,
                       std::span<std::optional<mr::TextureHandle>> textures,
                       mr::UniformBuffer &cam_ubo) noexcept
    : _ubo(state, ubo_data)
    , _shader(std::move(shader))
    , _descriptor_allocator(state)
{
  std::vector<Shader::ResourceView> attachments;
  attachments.reserve(textures.size());

  attachments.emplace_back(0, 0, &cam_ubo);
  attachments.emplace_back(0, 1, &_ubo);
  for (size_t i = 0; i < textures.size(); i++) {
    if (!textures[i].has_value()) {
      continue;
    }
    attachments.emplace_back(0, static_cast<uint32_t>(i + 2), textures[i].value().get());
  }

  auto layout_handle = ResourceManager<DescriptorSetLayout>::get().create(mr::unnamed,
    state, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, attachments);
  auto set_option =
    _descriptor_allocator
      .allocate_set(layout_handle);
  ASSERT(set_option.has_value());
  _descriptor_set = std::move(set_option.value());

  _descriptor_set.update(state, attachments);

  std::array layouts {layout_handle};
  _pipeline = mr::GraphicsPipeline(state,
                                   render_context,
                                   mr::GraphicsPipeline::Subpass::OpaqueGeometry,
                                   _shader,
                                   std::span {_descrs},
                                   std::span {layouts});
}

void mr::Material::bind(CommandUnit &unit) const noexcept
{
  unit->bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline.pipeline());
  unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                           {_pipeline.layout()},
                           0,
                           {_descriptor_set.set()},
                           {});
}
