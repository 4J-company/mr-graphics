#include "pch.hpp"
#include "resources/attachment/attachment.hpp"
#include "resources/pipelines/pipeline.hpp"

void mr::Pipeline::apply(vk::CommandBuffer cmd_buffer) const {}

mr::Pipeline::Pipeline(const VulkanState &state,
                       mr::ShaderHandle shader,
                       std::span<const DescriptorSetLayoutHandle> descriptor_layouts)
  : _shader(shader)
{
  ASSERT(shader.get(), "Shader should be valid");

  static constexpr size_t max_pipeline_layouts_number = 64;
  InplaceVector<vk::DescriptorSetLayout, max_pipeline_layouts_number> vk_descriptor_layouts;

  ASSERT(descriptor_layouts.size() < max_pipeline_layouts_number);

  vk_descriptor_layouts.clear();
  for (int i = 0; i < descriptor_layouts.size(); i++) {
    auto &layout = descriptor_layouts[i];
    ASSERT(layout.get());
    vk_descriptor_layouts.emplace_back(layout->layout());
  }

  vk::PipelineLayoutCreateInfo pipeline_layout_create_info {
    .setLayoutCount = static_cast<uint32_t>(vk_descriptor_layouts.size()),
    .pSetLayouts = vk_descriptor_layouts.data(),
  };

  auto [res, layout] = state.device().createPipelineLayoutUnique(pipeline_layout_create_info);
  assert(res == vk::Result::eSuccess);
  _layout = std::move(layout);
}
