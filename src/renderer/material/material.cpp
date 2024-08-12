#include "material/material.hpp"

mr::Material::Material(const VulkanState state, const vk::RenderPass render_pass,
                       Shader shader,
                       std::span<float> ubo_data,
                       std::span<std::optional<mr::Texture>> textures) noexcept
    : _ubo(state, ubo_data)
    , _shader(std::move(shader))
    , _descriptor_allocator(state)
{
  std::vector<Shader::ResourceView> attachments;
  attachments.reserve(textures.size());
  attachments.emplace_back(0, 0, &_ubo);
  for (unsigned int i = 0; i < textures.size(); i++) {
    if (!textures[i].has_value()) {
      continue;
    }
    attachments.emplace_back(0, i + 1, &textures[i].value());
  }

  _descriptor_set =
    _descriptor_allocator
      .allocate_set(Shader::Stage::Vertex, std::span {attachments})
      .value();

  std::array layouts = {_descriptor_set.layout()};

  _pipeline =
    mr::GraphicsPipeline(state,
                         render_pass,
                         mr::GraphicsPipeline::Subpass::OpaqueGeometry,
                         &_shader,
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
