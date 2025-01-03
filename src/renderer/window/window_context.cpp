#include "window_context.hpp"
#include "renderer.hpp"
#include "resources/buffer/buffer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "resources/descriptor/descriptor.hpp"
#include "resources/pipelines/graphics_pipeline.hpp"
#include "vkfw/vkfw.hpp"
#include <vulkan/vulkan_enums.hpp>

mr::RenderContext::RenderContext(VulkanGlobalState *state, Window *parent)
  : _parent(parent)
  , _state(state)
  , _extent(parent->extent())
  , _swapchain({}, {_state.device()})
{
  _surface = vkfw::createWindowSurfaceUnique(_state.instance(), _parent->window());

  _create_swapchain();
  _create_depthbuffer();
  _create_render_pass();
  _create_framebuffers();

  _image_available_semaphore = _state.device().createSemaphoreUnique({}).value;
  _render_finished_semaphore = _state.device().createSemaphoreUnique({}).value;
  _image_fence = _state.device().createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}).value;
}

mr::RenderContext::~RenderContext() {
  _state.queue().waitIdle();
}

void mr::RenderContext::_create_swapchain()
{
  vkb::SwapchainBuilder builder{_state.phys_device(), _state.device(), _surface.get()};
  auto swapchain = builder
    .set_desired_format({static_cast<VkFormat>(_swapchain_format), VK_COLORSPACE_SRGB_NONLINEAR_KHR})
    .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    .set_required_min_image_count(Framebuffer::max_presentable_images)
    .build();
  if (not swapchain) {
    std::cerr << "Cannot create VkSwapchainKHR: " << swapchain.error().message() << "\n";
  }

  _swapchain.reset(swapchain.value().swapchain);
}

void mr::RenderContext::_create_depthbuffer()
{
  auto format = Image::find_supported_format(
    _state,
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  _depthbuffer = Image(_state,
                       _extent,
                       format,
                       vk::ImageUsageFlagBits::eDepthStencilAttachment,
                       vk::ImageAspectFlagBits::eDepth);
  _depthbuffer.switch_layout(_state,
                             vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

void mr::RenderContext::_create_render_pass()
{
  for (unsigned i = 0; i < gbuffers_number; i++) {
    _gbuffers[i] = Image(_state,
                         _extent,
                         vk::Format::eR32G32B32A32Sfloat,
                         vk::ImageUsageFlagBits::eColorAttachment |
                           vk::ImageUsageFlagBits::eInputAttachment,
                         vk::ImageAspectFlagBits::eColor);
  }

  std::vector<vk::AttachmentDescription> color_attachments(
    gbuffers_number + 2,
    {
      .format = _gbuffers[0].format(),
      .samples = vk::SampleCountFlagBits::e1,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
      .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
      .initialLayout = vk::ImageLayout::eUndefined,
      .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    });

  color_attachments[0].format = _swapchain_format;
  color_attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;
  color_attachments.back().format = _depthbuffer.format();
  color_attachments.back().finalLayout =
    vk::ImageLayout::eDepthStencilAttachmentOptimal;

  const int subpass_number = 2;
  std::array<vk::SubpassDescription, subpass_number> subpasses;

  vk::AttachmentReference final_target_ref {
    .attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};
  std::array<vk::AttachmentReference, gbuffers_number> gbuffers_out_refs;
  for (uint i = 0; i < gbuffers_number; i++) {
    gbuffers_out_refs[i].attachment = i + 1;
    gbuffers_out_refs[i].layout = vk::ImageLayout::eColorAttachmentOptimal;
  }
  std::array<vk::AttachmentReference, gbuffers_number> gbuffers_in_refs;
  for (uint i = 0; i < gbuffers_number; i++) {
    gbuffers_in_refs[i].attachment = i + 1;
    gbuffers_in_refs[i].layout = vk::ImageLayout::eShaderReadOnlyOptimal;
  }

  vk::AttachmentReference depth_attachment_ref {
    .attachment = gbuffers_number + 1,
    .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
  };

  subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
  subpasses[0].colorAttachmentCount =
    static_cast<uint>(gbuffers_out_refs.size());
  subpasses[0].pColorAttachments = gbuffers_out_refs.data();
  subpasses[0].pDepthStencilAttachment = &depth_attachment_ref;

  subpasses[1].pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
  subpasses[1].inputAttachmentCount =
    static_cast<uint>(gbuffers_in_refs.size());
  subpasses[1].pInputAttachments = gbuffers_in_refs.data();
  subpasses[1].colorAttachmentCount = 1;
  subpasses[1].pColorAttachments = &final_target_ref;

  std::array<vk::SubpassDependency, subpass_number> dependencies;
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask =
    vk::PipelineStageFlagBits::eColorAttachmentOutput;
  dependencies[0].dstStageMask =
    vk::PipelineStageFlagBits::eColorAttachmentOutput;
  dependencies[0].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead |
                                  vk::AccessFlagBits::eColorAttachmentWrite;
  dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
  dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

  dependencies[1] = dependencies[0];
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = 1;

  vk::RenderPassCreateInfo render_pass_create_info {
    .attachmentCount = static_cast<uint>(color_attachments.size()),
    .pAttachments = color_attachments.data(),
    .subpassCount = static_cast<uint>(subpasses.size()),
    .pSubpasses = subpasses.data(),
    .dependencyCount = static_cast<uint>(dependencies.size()),
    .pDependencies = dependencies.data(),
  };

  _render_pass =
    _state.device().createRenderPassUnique(render_pass_create_info).value;
}

void mr::RenderContext::_create_framebuffers()
{
  // *** Swamp Chain Images� ***
  auto swampchain_images =
    _state.device().getSwapchainImagesKHR(_swapchain.get()).value;

  for (uint i = 0; i < Framebuffer::max_presentable_images; i++) {
    Image image{_state, _extent, _swapchain_format, swampchain_images[i], true};
    _framebuffers[i] = Framebuffer(_state,
                                   _render_pass.get(),
                                   _extent,
                                   std::move(image),
                                   _gbuffers,
                                   _depthbuffer);
  }
}

void mr::RenderContext::render()
{
  static Shader _shader = Shader(_state, "default");
  static std::vector<vk::VertexInputAttributeDescription> descrs {
    {.location = 0,
     .binding = 0,
     .format = vk::Format::eR32G32B32Sfloat,
     .offset = 0                },
    {.location = 1,
     .binding = 0,
     .format = vk::Format::eR32G32Sfloat,
     .offset = 3 * sizeof(float)}
  };

  const std::vector<vk::DescriptorSetLayoutBinding> bindings {
    {
     .binding = 0,
     .descriptorType = vk::DescriptorType::eCombinedImageSampler,
     .descriptorCount = 1,
     .stageFlags = vk::ShaderStageFlagBits::eFragment,
     },
    {
     .binding = 1,
     .descriptorType = vk::DescriptorType::eUniformBuffer,
     .descriptorCount = 1,
     .stageFlags = vk::ShaderStageFlagBits::eVertex,
     }
  };

  const float matr[16] {
    -1.41421354,
    0.816496611,
    -0.578506112,
    -0.577350259,
    0.00000000,
    -1.63299322,
    -0.578506112,
    -0.577350259,
    1.41421354,
    0.816496611,
    -0.578506112,
    -0.577350259,
    0.00000000,
    0.00000000,
    34.5101662,
    34.6410141,
  };

  static UniformBuffer uniform_buffer = UniformBuffer(_state, std::span {matr});
  static Texture texture = Texture(_state, "bin/textures/cat.png");
  static DescriptorAllocator descriptor_alloc = DescriptorAllocator(_state);
  static const std::vector<Shader::ResourceView> vertex_attachments {
    {0, 1, &uniform_buffer}, // set, binding, res
  };
  static const std::vector<Shader::ResourceView> fragment_attachments {
    {0, 0, &texture}, // set, binding, res
  };
  static const std::array attachments {
    std::make_pair(Shader::Stage::Vertex, std::span{vertex_attachments}),
    std::make_pair(Shader::Stage::Fragment, std::span {fragment_attachments}),
  };
  static auto sets = descriptor_alloc.allocate_sets(attachments).value();
  static const std::array layouts {sets[0].layout(), sets[1].layout() };
  static GraphicsPipeline pipeline = GraphicsPipeline(
    _state, _render_pass.get(), GraphicsPipeline::Subpass::OpaqueGeometry, &_shader, descrs, std::span{layouts});

  static CommandUnit command_unit {_state};

  struct vec3 {
      float x, y, z;
  };

  struct vec2 {
      float x, y;
  };

  struct vertex {
      vec3 coord;
      vec2 tex;
  };

  const std::vector<vertex> vertexes {
    { {-0.5, 0, -0.5}, {0, 0}},
    {  {0.5, 0, -0.5}, {1, 0}},
    {   {0.5, 0, 0.5}, {1, 1}},
    {  {-0.5, 0, 0.5}, {0, 1}},
    {{-0.5, -5, -0.5}, {0, 0}},
    { {0.5, -5, -0.5}, {1, 0}},
    {  {0.5, -5, 0.5}, {1, 1}},
    { {-0.5, -5, 0.5}, {0, 1}},
  };
  const std::vector indexes {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};
  static const VertexBuffer vertex_buffer =
    VertexBuffer(_state, std::span {vertexes});
  static const IndexBuffer index_buffer =
    IndexBuffer(_state, std::span {indexes});

  /// light
  const std::vector<float> light_vertexes {-1, -1, 1, -1, 1, 1, -1, 1};
  const std::vector light_indexes {0, 1, 2, 2, 3, 0};
  static const VertexBuffer light_vertex_buffer =
    VertexBuffer(_state, std::span {light_vertexes});
  static const IndexBuffer light_index_buffer =
    IndexBuffer(_state, std::span {light_indexes});
  vk::VertexInputAttributeDescription light_descr {.location = 0,
                                                   .binding = 0,
                                                   .format =
                                                     vk::Format::eR32G32Sfloat,
                                                   .offset = 0};
  static Shader light_shader = Shader(_state, "light");
  std::vector<Shader::ResourceView> light_attach;
  light_attach.reserve(gbuffers_number);
  for (unsigned i = 0; i < gbuffers_number; i++) {
    light_attach.emplace_back(0, i, &_gbuffers[i]);
  }
  static DescriptorSet light_set =
    descriptor_alloc.allocate_set(Shader::Stage::Fragment, light_attach).value_or(DescriptorSet());
  static vk::DescriptorSetLayout light_layout = light_set.layout();
  static GraphicsPipeline light_pipeline = GraphicsPipeline(_state,
                                                            _render_pass.get(),
                                                            GraphicsPipeline::Subpass::OpaqueLighting,
                                                            &light_shader,
                                                            {&light_descr, 1},
                                                            {&light_layout, 1});

  _state.device().waitForFences(_image_fence.get(), VK_TRUE, UINT64_MAX);
  _state.device().resetFences(_image_fence.get());

  mr::uint image_index = 0;
  _state.device().acquireNextImageKHR(_swapchain.get(),
                                      UINT64_MAX,
                                      _image_available_semaphore.get(),
                                      nullptr,
                                      &image_index);
  command_unit.begin();

  vk::ClearValue clear_color {vk::ClearColorValue(
    std::array {0, 0, 0, 0})}; // anyone who changes that line will be fucked
  std::array<vk::ClearValue, gbuffers_number + 2> clear_colors {};
  for (unsigned i = 0; i < gbuffers_number + 1; i++) {
    clear_colors[i].color = clear_color.color;
  }
  clear_colors.back().depthStencil = vk::ClearDepthStencilValue {1.0f, 0};

  vk::RenderPassBeginInfo render_pass_info {
    .renderPass = _render_pass.get(),
    .framebuffer = _framebuffers[image_index].framebuffer(),
    .renderArea = {{0, 0}, _extent},
    .clearValueCount = static_cast<uint>(clear_colors.size()),
    .pClearValues = clear_colors.data(),
  };

  command_unit->beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
  command_unit->bindPipeline(vk::PipelineBindPoint::eGraphics,
                             pipeline.pipeline());
  command_unit->setViewport(0, _framebuffers[image_index].viewport());
  command_unit->setScissor(0, _framebuffers[image_index].scissors());
  command_unit->bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics, pipeline.layout(), 0, {sets[0].set(), sets[1].set()}, {});
  command_unit->bindVertexBuffers(0, {vertex_buffer.buffer()}, {0});
  command_unit->bindIndexBuffer(
    index_buffer.buffer(), 0, vk::IndexType::eUint32);
  command_unit->drawIndexed(indexes.size(), 1, 0, 0, 0);
  command_unit->nextSubpass(vk::SubpassContents::eInline);
  command_unit->bindPipeline(vk::PipelineBindPoint::eGraphics,
                             light_pipeline.pipeline());
  command_unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                   light_pipeline.layout(),
                                   0,
                                   {light_set.set()},
                                   {});
  command_unit->bindVertexBuffers(0, {light_vertex_buffer.buffer()}, {0});
  command_unit->bindIndexBuffer(
    light_index_buffer.buffer(), 0, vk::IndexType::eUint32);
  command_unit->drawIndexed(light_indexes.size(), 1, 0, 0, 0);
  command_unit->endRenderPass();

  command_unit.end();

  vk::Semaphore wait_semaphores[] = {_image_available_semaphore.get()};
  vk::PipelineStageFlags wait_stages[] = {
    vk::PipelineStageFlagBits::eColorAttachmentOutput};
  vk::Semaphore signal_semaphores[] = {_render_finished_semaphore.get()};

  auto [bufs, size] = command_unit.submit_info();
  vk::SubmitInfo submit_info {
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = wait_semaphores,
    .pWaitDstStageMask = wait_stages,
    .commandBufferCount = size,
    .pCommandBuffers = bufs,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = signal_semaphores,
  };

  _state.queue().submit(submit_info, _image_fence.get());

  vk::SwapchainKHR swapchains[] = {_swapchain.get()};
  vk::PresentInfoKHR present_info {
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = signal_semaphores,
    .swapchainCount = 1,
    .pSwapchains = swapchains,
    .pImageIndices = &image_index,
    .pResults = nullptr, // Optional
  };
  _state.queue().presentKHR(present_info);
}
