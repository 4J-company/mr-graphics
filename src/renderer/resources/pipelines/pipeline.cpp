#include "resources/pipelines/pipeline.hpp"

void mr::Pipeline::apply(vk::CommandBuffer cmd_buffer) const {}

mr::Pipeline::Pipeline(
    const VulkanState &state, Shader *shader,
    const std::span<vk::DescriptorSetLayout> descriptor_layouts) : _shader(shader)
{
  vk::PipelineLayoutCreateInfo pipeline_layout_create_info {
    .setLayoutCount = static_cast<uint>(descriptor_layouts.size()),
    .pSetLayouts = descriptor_layouts.data(),
  };
  auto [res, layout] = state.device().createPipelineLayoutUnique(pipeline_layout_create_info);
  assert(res == vk::Result::eSuccess);
  _layout = std::move(layout);
}
