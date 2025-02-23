#include "resources/pipelines/pipeline.hpp"

void mr::Pipeline::apply(vk::CommandBuffer cmd_buffer) const {}

/**
 * @brief Constructs a Pipeline object by creating a Vulkan pipeline layout.
 *
 * This constructor initializes a new Pipeline using the provided shader handle and the specified descriptor set layouts.
 * It constructs a pipeline layout creation structure from the descriptor layouts and uses the device from the given state
 * to create a unique pipeline layout. The creation result is verified to ensure success.
 *
 * @param state Reference to the VulkanState containing the device used for pipeline layout creation.
 * @param shader Shader handle associated with this pipeline.
 * @param descriptor_layouts Span of descriptor set layouts defining the pipeline's layout.
 */
mr::Pipeline::Pipeline(
    const VulkanState &state, mr::ShaderHandle shader,
    std::span<const vk::DescriptorSetLayout> descriptor_layouts) : _shader(shader)
{
  vk::PipelineLayoutCreateInfo pipeline_layout_create_info {
    .setLayoutCount = static_cast<uint>(descriptor_layouts.size()),
    .pSetLayouts = descriptor_layouts.data(),
  };
  auto [res, layout] = state.device().createPipelineLayoutUnique(pipeline_layout_create_info);
  assert(res == vk::Result::eSuccess);
  _layout = std::move(layout);
}
