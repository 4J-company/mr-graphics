#include "resources/pipelines/graphics_pipeline.hpp"

mr::GraphicsPipeline::GraphicsPipeline(const VulkanState &state, Shader *shader)
{
  _shader = shader;

  // dynamic states of pipeline (viewport)
  _dynamic_states.push_back(vk::DynamicState::eViewport);
  _dynamic_states.push_back(vk::DynamicState::eScissor);
  vk::PipelineDynamicStateCreateInfo dynamic_state_create_info {.dynamicStateCount =
                                                                    static_cast<uint>(_dynamic_states.size()),
                                                                .pDynamicStates = _dynamic_states.data()};

  vk::PipelineVertexInputStateCreateInfo vertex_input_create_info {.vertexBindingDescriptionCount = 0,
                                                                   .pVertexBindingDescriptions = nullptr,
                                                                   .vertexAttributeDescriptionCount = 0,
                                                                   .pVertexAttributeDescriptions = nullptr};

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

  // create pipeline layout
  vk::PipelineLayoutCreateInfo pipeline_layout_create_info {
      .setLayoutCount = 0,
      .pSetLayouts = nullptr,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };
  _layout = state.device().createPipelineLayout(pipeline_layout_create_info).value;

  // retrieve pipeline cache
  size_t cache_size = 0;
  char *cache_data = nullptr;
  std::fstream cache_file("bin/cache/pipeline", std::ios::in | std::ios::out | std::ios::binary | std::ios::ate);
  if (cache_file)
  {
    cache_size = cache_file.tellg();
    cache_data = new char[cache_size];
    cache_file.seekg(std::ios::beg);
    cache_file.read(cache_data, cache_size);
    // TODO: maybe cache validation is needed (vk::PipelineCacheHeaderVersionOne)
  }
  vk::PipelineCacheCreateInfo pipeline_cache_create_info {.initialDataSize = cache_size, .pInitialData = cache_data};
  _pipeline_cache = state.device().createPipelineCache(pipeline_cache_create_info).value;
  if (cache_data != nullptr)
    delete[] cache_data;

  // create pipeline
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
      .layout = _layout,
      .renderPass = state.render_pass(),
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE, // Optional
      .basePipelineIndex = -1,              // Optional
  };

  std::vector<vk::Pipeline> pipelines;
  pipelines = state.device().createGraphicsPipelines(_pipeline_cache, pipeline_create_info).value;
  _pipeline = pipelines[0];

  // save pipeline cache
  auto result = state.device().getPipelineCacheData(_pipeline_cache, &cache_size, nullptr);
  assert(result == vk::Result::eSuccess);
  char *cache_save_data = new char[cache_size]; // TODO: maybe using cache_data buffer instead
  result = state.device().getPipelineCacheData(_pipeline_cache, &cache_size, cache_save_data);
  assert(result == vk::Result::eSuccess);
  if (cache_file && cache_save_data != nullptr)
  {
    cache_file.seekg(std::ios::beg);
    cache_file.write(cache_save_data, cache_size);
  }
  if (cache_save_data != nullptr)
    delete[] cache_save_data;
}

void mr::GraphicsPipeline::apply(vk::CommandBuffer cmd_buffer) const
{
  cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
}

void mr::GraphicsPipeline::recompile() {}
