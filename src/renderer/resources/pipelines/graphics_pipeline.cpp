module;
#include "pch.hpp"
export module GraphicsPipeline;

import Pipeline;

export namespace mr {
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

      GraphicsPipeline(
        const VulkanState &state, vk::RenderPass render_pass, uint subpass,
        Shader *ShaderProgram,
        const std::vector<vk::VertexInputAttributeDescription> &attributes,
        const std::vector<std::vector<vk::DescriptorSetLayoutBinding>> &bindings);

      void recompile();

      void apply(vk::CommandBuffer cmd_buffer) const override;
  };
} // namespace mr

mr::GraphicsPipeline::GraphicsPipeline(
  const VulkanState &state, vk::RenderPass render_pass, uint subpass,
  Shader *shader, const std::vector<vk::VertexInputAttributeDescription> &attributes,
  const std::vector<std::vector<vk::DescriptorSetLayoutBinding>> &bindings)
    : Pipeline(state, shader, bindings)
    , _subpass(subpass)
{
  // dynamic states of pipeline (viewport)
  _dynamic_states.push_back(vk::DynamicState::eViewport);
  _dynamic_states.push_back(vk::DynamicState::eScissor);
  vk::PipelineDynamicStateCreateInfo dynamic_state_create_info {
    .dynamicStateCount = static_cast<uint>(_dynamic_states.size()),
    .pDynamicStates = _dynamic_states.data()};

  uint size = 0;
  for (auto &atr : attributes) {
    size += atr.format == vk::Format::eR32G32B32Sfloat   ? 4
            : atr.format == vk::Format::eR32G32B32Sfloat ? 3
            : atr.format == vk::Format::eR32G32Sfloat    ? 2
                                                         : 1;
  }
  size *= 4;

  vk::VertexInputBindingDescription binding_description {
    .binding = 0,
    .stride = size,
    .inputRate = vk::VertexInputRate::eVertex,
  };

  vk::PipelineVertexInputStateCreateInfo vertex_input_create_info {
    .vertexBindingDescriptionCount = static_cast<bool>(attributes.size()),
    .pVertexBindingDescriptions = &binding_description,
    .vertexAttributeDescriptionCount = static_cast<uint>(attributes.size()),
    .pVertexAttributeDescriptions = attributes.data()};

  _topology = vk::PrimitiveTopology::eTriangleList;
  vk::PipelineInputAssemblyStateCreateInfo input_assembly_create_info {
    .topology = _topology, .primitiveRestartEnable = false};

  vk::PipelineViewportStateCreateInfo viewport_state_create_info {
    .viewportCount = 1, .scissorCount = 1};

  vk::PipelineRasterizationStateCreateInfo rasterizer_create_info {
    .depthClampEnable = false,
    .rasterizerDiscardEnable = false,
    .polygonMode = vk::PolygonMode::eFill,
    .cullMode = vk::CullModeFlagBits::eNone,
    .frontFace = vk::FrontFace::eClockwise,
    .depthBiasEnable = false,
    .depthBiasConstantFactor = 0.0f,
    .depthBiasClamp = 0.0f,
    .depthBiasSlopeFactor = 0.0f,
    .lineWidth = 1.0f,
  };

  vk::PipelineMultisampleStateCreateInfo multisampling_create_info {
    .rasterizationSamples = vk::SampleCountFlagBits::e1,
    .sampleShadingEnable = false,
    .minSampleShading = 1.0f,
    .pSampleMask = nullptr,
    .alphaToCoverageEnable = false,
    .alphaToOneEnable = false};

  vk::PipelineDepthStencilStateCreateInfo depth_stencil_create_info {
    .depthTestEnable = true,
    .depthWriteEnable = true,
    .depthCompareOp = vk::CompareOp::eLess,
    .depthBoundsTestEnable = false,
    .stencilTestEnable = false,
    .front = {},
    .back = {},
    .minDepthBounds = 0.0f,
    .maxDepthBounds = 1.0f,
  };

  std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachments;
  switch (subpass) {
    case 0:
      color_blend_attachments.resize(WindowContext::gbuffers_number);
      for (unsigned i = 0; i < WindowContext::gbuffers_number; i++) {
        color_blend_attachments[i].blendEnable = false;
        color_blend_attachments[i].srcColorBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachments[i].dstColorBlendFactor = vk::BlendFactor::eZero;
        color_blend_attachments[i].colorBlendOp = vk::BlendOp::eAdd;
        color_blend_attachments[i].srcAlphaBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachments[i].dstAlphaBlendFactor = vk::BlendFactor::eZero;
        color_blend_attachments[i].alphaBlendOp = vk::BlendOp::eAdd;
        color_blend_attachments[i].colorWriteMask =
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
      }
      break;
    case 1:
      color_blend_attachments.resize(1);
      color_blend_attachments[0].blendEnable = false;
      color_blend_attachments[0].srcColorBlendFactor = vk::BlendFactor::eOne;
      color_blend_attachments[0].dstColorBlendFactor = vk::BlendFactor::eZero;
      color_blend_attachments[0].colorBlendOp = vk::BlendOp::eAdd;
      color_blend_attachments[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
      color_blend_attachments[0].dstAlphaBlendFactor = vk::BlendFactor::eZero;
      color_blend_attachments[0].alphaBlendOp = vk::BlendOp::eAdd;
      color_blend_attachments[0].colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  }

  vk::PipelineColorBlendStateCreateInfo color_blending_create_info {
    .logicOpEnable = false,
    .logicOp = vk::LogicOp::eCopy,
    .attachmentCount = static_cast<uint>(color_blend_attachments.size()),
    .pAttachments = color_blend_attachments.data(),
    .blendConstants = std::array<float, 4> {0.0, 0.0, 0.0, 0.0}, // ???
  };

  vk::GraphicsPipelineCreateInfo pipeline_create_info {
    .stageCount = _shader->stage_number(),
    .pStages = _shader->get_stages().data(),
    .pVertexInputState = &vertex_input_create_info,
    .pInputAssemblyState = &input_assembly_create_info,
    .pViewportState = &viewport_state_create_info,
    .pRasterizationState = &rasterizer_create_info,
    .pMultisampleState = &multisampling_create_info,
    .pColorBlendState = &color_blending_create_info,
    .pDynamicState = &dynamic_state_create_info,
    .layout = _layout.get(),
    .renderPass = render_pass,
    .subpass = _subpass,
    .basePipelineHandle = VK_NULL_HANDLE, // Optional
    .basePipelineIndex = -1,              // Optional
  };
  if (_subpass == 0) {
    pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
  }

  _pipeline = std::move(state.device()
                          .createGraphicsPipelinesUnique(state.pipeline_cache(),
                                                         pipeline_create_info)
                          .value[0]);
}

void mr::GraphicsPipeline::apply(vk::CommandBuffer cmd_buffer) const
{
  cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline.get());
}

void mr::GraphicsPipeline::recompile() {}
