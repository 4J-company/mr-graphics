#include "material/material.hpp"

mr::graphics::Material::Material(const VulkanState &state,
                       const RenderContext &render_context,
                       mr::graphics::ShaderHandle shader,
                       std::span<std::byte> ubo_data,
                       std::span<std::optional<mr::TextureHandle>> textures,
                       std::span<mr::StorageBuffer*> storage_buffers,
                       std::span<mr::ConditionalBuffer*> conditional_buffers,
                       mr::UniformBuffer &cam_ubo) noexcept
    : _ubo(state, ubo_data)
    , _shader(shader)
    , _descriptor_allocator(state)
{
  ASSERT(_shader.get() != nullptr, "Invalid shader passed to the material", _shader->name());

  std::vector<Shader::ResourceView> attachments;
  attachments.reserve(textures.size() + storage_buffers.size() + conditional_buffers.size() + 2);

  attachments.emplace_back(0, 0, &cam_ubo);
  attachments.emplace_back(0, 1, &_ubo);
  for (size_t i = 0; i < textures.size(); i++) {
    if (!textures[i].has_value()) {
      continue;
    }
    attachments.emplace_back(0, static_cast<uint32_t>(i + 2), textures[i]->get());
  }
  for (size_t i = 0; i < storage_buffers.size(); i++) {
    attachments.emplace_back(0, static_cast<uint32_t>(i + textures.size() + 2), storage_buffers[i]);
  }
  for (size_t i = 0; i < conditional_buffers.size(); i++) {
    attachments.emplace_back(
      0,
      static_cast<uint32_t>(i + textures.size() + storage_buffers.size() + 2),
      conditional_buffers[i]
    );
  }

  auto layout_handle = ResourceManager<DescriptorSetLayout>::get().create(shader->name(),
    state, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, attachments);
  _descriptor_set = *ASSERT_VAL(_descriptor_allocator.allocate_set(layout_handle));

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
