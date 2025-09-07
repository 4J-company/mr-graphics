#include "resources/pipelines/graphics_pipeline.hpp"

mr::GraphicsPipeline::GraphicsPipeline(const VulkanState &state,
                                       const RenderContext &render_context,
                                       Subpass subpass,
                                       mr::ShaderHandle shader,
                                       std::span<const vk::VertexInputAttributeDescription> attributes,
                                       std::span<const DescriptorSetLayoutHandle> descriptor_layouts)
  : Pipeline(state, shader, descriptor_layouts), _subpass(subpass)
{
  // dynamic states of pipeline (viewport)
  _dynamic_states.push_back(vk::DynamicState::eViewport);
  _dynamic_states.push_back(vk::DynamicState::eScissor);
  vk::PipelineDynamicStateCreateInfo dynamic_state_create_info {
    .dynamicStateCount = static_cast<uint>(_dynamic_states.size()),
    .pDynamicStates = _dynamic_states.data()};

  auto fmt2uint = [](vk::Format fmt) -> uint32_t {
    return sizeof(float) * (
      fmt == vk::Format::eR32G32B32A32Sfloat ? 4
      : fmt == vk::Format::eR32G32B32Sfloat  ? 3
      : fmt == vk::Format::eR32G32Sfloat     ? 2
                                             : 1);
  };

  std::array<vk::VertexInputBindingDescription, 16> binding_descriptions {};
  for (int i = 0; i < attributes.size(); i++) {
    binding_descriptions[i] = vk::VertexInputBindingDescription {
      .binding = attributes[i].binding,
      .stride = fmt2uint(attributes[i].format),
      .inputRate = vk::VertexInputRate::eVertex,
    };
  }

  vk::PipelineVertexInputStateCreateInfo vertex_input_create_info {
    .vertexBindingDescriptionCount = (uint)attributes.size(),
    .pVertexBindingDescriptions = binding_descriptions.data(),
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
    .alphaToOneEnable = false
  };

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

  vk::PipelineRenderingCreateInfoKHR pipiline_rendering_create_info {};

  std::array<vk::PipelineColorBlendAttachmentState, RenderContext::gbuffers_number> color_blend_attachments;
  std::array<vk::Format, RenderContext::gbuffers_number> color_attachments_formats;
  uint32_t color_attacmhents_cnt = 0;

  switch (subpass) {
    case Subpass::OpaqueGeometry:
      color_attacmhents_cnt = RenderContext::gbuffers_number;

      for (int i = 0; i < RenderContext::gbuffers_number; i++) {
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

      for (int i = 0; i < RenderContext::gbuffers_number; i++) {
        // TODO(dk6): remove magic number
        color_attachments_formats[i] = vk::Format::eR32G32B32A32Sfloat;
      }
      pipiline_rendering_create_info.depthAttachmentFormat = mr::get_depthbuffer_format(state);

      break;
    case Subpass::OpaqueLighting:
      color_attacmhents_cnt = 1;

      color_blend_attachments[0].blendEnable = true;
      color_blend_attachments[0].srcColorBlendFactor = vk::BlendFactor::eOne;
      color_blend_attachments[0].dstColorBlendFactor = vk::BlendFactor::eOne;
      color_blend_attachments[0].colorBlendOp = vk::BlendOp::eAdd;

      // Alpha doesn't matter now
      color_blend_attachments[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
      color_blend_attachments[0].dstAlphaBlendFactor = vk::BlendFactor::eOne;
      color_blend_attachments[0].alphaBlendOp = vk::BlendOp::eAdd;

      color_blend_attachments[0].colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

      color_attachments_formats[0] = mr::get_swapchain_format(state);

      break;
    default:
      ASSERT(false, "Invalid subpass option");
  }

  pipiline_rendering_create_info.colorAttachmentCount = color_attacmhents_cnt;
  pipiline_rendering_create_info.pColorAttachmentFormats = color_attachments_formats.data();

  vk::PipelineColorBlendStateCreateInfo color_blending_create_info {
    .logicOpEnable = false,
    .logicOp = vk::LogicOp::eCopy,
    .attachmentCount = static_cast<uint32_t>(color_attacmhents_cnt),
    .pAttachments = color_blend_attachments.data(),
    .blendConstants = std::array<float, 4> {0.0, 0.0, 0.0, 0.0}, // ???
  };

  vk::StructureChain chain {
    vk::GraphicsPipelineCreateInfo {
      .stageCount = _shader->stage_number(),
      .pStages = _shader->stages().data(),
      .pVertexInputState = &vertex_input_create_info,
      .pInputAssemblyState = &input_assembly_create_info,
      .pViewportState = &viewport_state_create_info,
      .pRasterizationState = &rasterizer_create_info,
      .pMultisampleState = &multisampling_create_info,
      .pDepthStencilState = &depth_stencil_create_info,
      .pColorBlendState = &color_blending_create_info,
      .pDynamicState = &dynamic_state_create_info,
      .layout = _layout.get(),
    },
    pipiline_rendering_create_info,
  };

  _pipeline = std::move(state.device()
                          .createGraphicsPipelinesUnique(state.pipeline_cache(), chain.get())
                          .value[0]);
}

void mr::GraphicsPipeline::apply(vk::CommandBuffer cmd_buffer) const
{
  cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline.get());
}

void mr::GraphicsPipeline::recompile() {}
