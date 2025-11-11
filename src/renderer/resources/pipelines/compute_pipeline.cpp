#include "compute_pipeline.hpp"
#include "renderer/window/render_context.hpp"

mr::ComputePipeline::ComputePipeline(const VulkanState &state,
                                     mr::ShaderHandle shader,
                                     std::span<const DescriptorSetLayoutHandle> descriptor_layouts)
  : Pipeline(state, shader, descriptor_layouts, compute_pipeline_push_constants)
{
  ASSERT(shader->stage_number() == 1, "Compute shader have only one stage");
  vk::ComputePipelineCreateInfo create_info {
    .stage = shader->stages().front(),
    .layout = _layout.get()
  };

  auto &&[res, pipeline] = state.device().createComputePipelineUnique({}, create_info);
  ASSERT(res == vk::Result::eSuccess, "Error to create compute pipeline", shader->name());
  _pipeline = std::move(pipeline);
}
