#ifndef __graphics_pipeline_hpp_
#define __graphics_pipeline_hpp_

#include "resources/pipelines/pipeline.hpp"

namespace ter
{
  class GraphicsPipeline : public Pipeline
  {
  private:
    uint _subpass;

    vk::PrimitiveTopology _topology;
    vk::VertexInputBindingDescription _binding_descriptor;

    uint PipelineParametersFlag;

    std::vector<vk::DynamicState> _dynamic_states;
    std::vector<vk::DescriptorSetLayout> _layouts_descriptors;

  public:
    GraphicsPipeline() = default;
    ~GraphicsPipeline() = default;

    GraphicsPipeline(VulkanState va, Shader *ShaderProgram);

    GraphicsPipeline(GraphicsPipeline &&other) noexcept = default;
    GraphicsPipeline &operator=(GraphicsPipeline &&other) noexcept = default;

    void recompile();

    void apply(vk::CommandBuffer cmd_buffer) const override;
  };
} // namespace ter
#endif // __graphics_pipeline_hpp_
