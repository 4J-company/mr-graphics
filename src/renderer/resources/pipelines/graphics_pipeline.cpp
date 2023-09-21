#include "resources/pipelines/graphics_pipeline.hpp"

mr::GraphicsPipeline::GraphicsPipeline(const VulkanState &state, Shader *shader, std::vector<vk::VertexInputAttributeDescription> attributes,
                                       std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings) : Pipeline(state, shader, bindings)
{
  // dynamic states of pipeline (viewport)
  _dynamic_states.push_back(vk::DynamicState::eViewport);
  _dynamic_states.push_back(vk::DynamicState::eScissor);
  vk::PipelineDynamicStateCreateInfo dynamic_state_create_info {.dynamicStateCount =
                                                                    static_cast<uint>(_dynamic_states.size()),
                                                                .pDynamicStates = _dynamic_states.data()};

  uint size = 0;
  for (auto &atr : attributes)
    size += atr.format == vk::Format::eR32G32B32Sfloat ? 4 :
            atr.format == vk::Format::eR32G32B32Sfloat ? 3 :
            atr.format == vk::Format::eR32G32Sfloat ? 2 : 1;
  size *= 4;

  vk::VertexInputBindingDescription binding_description
  {
    .binding = 0,
    .stride = size,
    .inputRate = vk::VertexInputRate::eVertex,
  };

  vk::PipelineVertexInputStateCreateInfo vertex_input_create_info {.vertexBindingDescriptionCount = static_cast<bool>(attributes.size()),
                                                                   .pVertexBindingDescriptions = &binding_description,
                                                                   .vertexAttributeDescriptionCount = static_cast<uint>(attributes.size()),
                                                                   .pVertexAttributeDescriptions = attributes.data()};

  _topology = vk::PrimitiveTopology::eTriangleList;
  vk::PipelineInputAssemblyStateCreateInfo input_assembly_create_info {.topology = _topology,
                                                                       .primitiveRestartEnable = false};

  vk::PipelineViewportStateCreateInfo viewport_state_create_info {.viewportCount = 1, .scissorCount = 1};

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

  vk::PipelineMultisampleStateCreateInfo multisampling_create_info {.rasterizationSamples = vk::SampleCountFlagBits::e1,
                                                                    .sampleShadingEnable = false,
                                                                    .minSampleShading = 1.0f,
                                                                    .pSampleMask = nullptr,
                                                                    .alphaToCoverageEnable = false,
                                                                    .alphaToOneEnable = false};

  vk::PipelineColorBlendAttachmentState color_blend_attachment {
      .blendEnable = false,
      .srcColorBlendFactor = vk::BlendFactor::eOne,
      .dstColorBlendFactor = vk::BlendFactor::eZero,
      .colorBlendOp = vk::BlendOp::eAdd,
      .srcAlphaBlendFactor = vk::BlendFactor::eOne,
      .dstAlphaBlendFactor = vk::BlendFactor::eZero,
      .alphaBlendOp = vk::BlendOp::eAdd,
      .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
  };

  vk::PipelineColorBlendStateCreateInfo color_blendin_create_info {
      .logicOpEnable = false,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment,
      .blendConstants = std::array<float, 4> {0.0, 0.0, 0.0, 0.0}, // ???
  };

  vk::GraphicsPipelineCreateInfo pipeline_create_info {
      // .stageCount = static_cast<uint>(_shader->get_stages().size()),
      .stageCount = _shader->stage_number(),
      .pStages = _shader->get_stages().data(),
      .pVertexInputState = &vertex_input_create_info,
      .pInputAssemblyState = &input_assembly_create_info,
      .pViewportState = &viewport_state_create_info,
      .pRasterizationState = &rasterizer_create_info,
      .pMultisampleState = &multisampling_create_info,
      .pDepthStencilState = nullptr,
      .pColorBlendState = &color_blendin_create_info,
      .pDynamicState = &dynamic_state_create_info,
      .layout = _layout.get(),
      .renderPass = state.render_pass(),
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE, // Optional
      .basePipelineIndex = -1,              // Optional
  };

  std::vector<vk::Pipeline> pipelines;
  pipelines = state.device().createGraphicsPipelines(nullptr, pipeline_create_info).value;
  _pipeline.reset(pipelines[0]);
}

void mr::GraphicsPipeline::apply(vk::CommandBuffer cmd_buffer) const
{
  cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline.get());
}

void mr::GraphicsPipeline::recompile() {}
