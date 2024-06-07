#ifndef __graphics_pipeline_hpp_
#define __graphics_pipeline_hpp_

#include "resources/pipelines/pipeline.hpp"

namespace mr {
  class GraphicsPipeline : public Pipeline {
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

      GraphicsPipeline(
        const VulkanState &state, vk::RenderPass render_pass, uint subpass,
        Shader *ShaderProgram,
        std::vector<vk::VertexInputAttributeDescription> attributes,
        std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings);

      void recompile();

      void apply(vk::CommandBuffer cmd_buffer) const override;
  };
} // namespace mr
#endif // __graphics_pipeline_hpp_
