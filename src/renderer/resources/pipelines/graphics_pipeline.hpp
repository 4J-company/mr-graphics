#ifndef __MR_GRAPHICS_PIPELINE_HPP_
#define __MR_GRAPHICS_PIPELINE_HPP_

#include "resources/pipelines/pipeline.hpp"
#include "resources/shaders/shader.hpp"

namespace mr {
inline namespace graphics {
  class GraphicsPipeline : public Pipeline, public ResourceBase<GraphicsPipeline> {
    public:
      enum struct Subpass {
        OpaqueGeometry = 0,
        OpaqueLighting = 1,
      };

    private:
      constexpr static inline vk::PushConstantRange graphics_pipeline_push_constants {
        .stageFlags = vk::ShaderStageFlagBits::eAllGraphics,
        .offset = 0,
        .size = 48,
      };

      Subpass _subpass;

      vk::PrimitiveTopology _topology;
      vk::VertexInputBindingDescription _binding_descriptor;

      uint PipelineParametersFlag;

      std::vector<vk::DynamicState> _dynamic_states;
      std::vector<vk::DescriptorSetLayout> _layouts_descriptors;

    public:
      GraphicsPipeline() = default;

      GraphicsPipeline(const RenderContext &render_context,
                       Subpass subpass,
                       mr::ShaderHandle shader,
                       std::span<const vk::VertexInputAttributeDescription> attributes,
                       std::span<const DescriptorSetLayoutHandle> descriptor_layouts);

      void recompile();

      void apply(vk::CommandBuffer cmd_buffer) const override;
  };
  MR_DECLARE_HANDLE(GraphicsPipeline);
}
} // namespace mr
#endif // __MR_GRAPHICS_PIPELINE_HPP_
