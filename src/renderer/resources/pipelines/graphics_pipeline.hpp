#ifndef __MR_GRAPHICS_PIPELINE_HPP_
#define __MR_GRAPHICS_PIPELINE_HPP_

#include "resources/pipelines/pipeline.hpp"

namespace mr {
  class GraphicsPipeline : public Pipeline {
    public:
      enum struct Subpass {
        OpaqueGeometry = 0,
        OpaqueLighting = 1,
      };

    private:
      Subpass _subpass;

      vk::PrimitiveTopology _topology;
      vk::VertexInputBindingDescription _binding_descriptor;

      uint PipelineParametersFlag;

      std::vector<vk::DynamicState> _dynamic_states;
      std::vector<vk::DescriptorSetLayout> _layouts_descriptors;

    public:
      GraphicsPipeline() = default;

      GraphicsPipeline(const VulkanState &state,
                       const RenderContext &render_context,
                       Subpass subpass,
                       mr::ShaderHandle shader,
                       std::span<const vk::VertexInputAttributeDescription> attributes,
                       std::span<const vk::DescriptorSetLayout> descriptor_layouts);

      void recompile();

      void apply(vk::CommandBuffer cmd_buffer) const override;
  };
} // namespace mr
#endif // __MR_GRAPHICS_PIPELINE_HPP_
