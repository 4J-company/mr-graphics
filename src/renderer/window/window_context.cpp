#include "window_context.hpp"
#include "renderer.hpp"
#include "resources/buffer/buffer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "resources/pipelines/graphics_pipeline.hpp"
#include "vkfw/vkfw.hpp"
#include <vulkan/vulkan_enums.hpp>

mr::WindowContext::WindowContext(Window *parent, const VulkanState &state)
    : _state(state)
    , _parent(parent)
{
  _surface =
    vkfw::createWindowSurfaceUnique(state.instance(), parent->window());

  size_t universal_queue_family_id = 0;

  // Choose surface format
  const auto avaible_formats =
    state.phys_device().getSurfaceFormatsKHR(_surface.get()).value;
  _swapchain_format = avaible_formats[0].format;
  for (const auto format : avaible_formats) {
    if (format.format == vk::Format::eB8G8R8A8Unorm) // Preferred format
    {
      _swapchain_format = format.format;
      break;
    }
    else if (format.format == vk::Format::eR8G8B8A8Unorm) {
      _swapchain_format = format.format;
    }
  }

  // Choose surface extent
  vk::SurfaceCapabilitiesKHR surface_capabilities;
  surface_capabilities =
    state.phys_device().getSurfaceCapabilitiesKHR(_surface.get()).value;
  if (surface_capabilities.currentExtent.width ==
      std::numeric_limits<uint>::max()) {
    // If the surface size is undefined, the size is set to the size of the images requested.
    _extent.width = std::clamp(static_cast<uint32_t>(_parent->width()),
                               surface_capabilities.minImageExtent.width,
                               surface_capabilities.maxImageExtent.width);
    _extent.height = std::clamp(static_cast<uint32_t>(_parent->height()),
                                surface_capabilities.minImageExtent.height,
                                surface_capabilities.maxImageExtent.height);
  }
  else { // If the surface size is defined, the swap chain size must match
    _extent = surface_capabilities.currentExtent;
  }

  // Choose surface present mode
  const auto avaible_present_modes =
    state.phys_device().getSurfacePresentModesKHR(_surface.get()).value;
  const auto iter = std::find(avaible_present_modes.begin(),
                              avaible_present_modes.end(),
                              vk::PresentModeKHR::eMailbox);
  auto present_mode =
    (iter != avaible_present_modes.end())
      ? *iter
      : vk::PresentModeKHR::eFifo; // FIFO is required to be supported (by spec)

  // Choose surface transform
  vk::SurfaceTransformFlagBitsKHR pre_transform =
    (surface_capabilities.supportedTransforms &
     vk::SurfaceTransformFlagBitsKHR::eIdentity)
      ? vk::SurfaceTransformFlagBitsKHR::eIdentity
      : surface_capabilities.currentTransform;

  // Choose composite alpha
  vk::CompositeAlphaFlagBitsKHR composite_alpha =
    (surface_capabilities.supportedCompositeAlpha &
     vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
      ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
    : (surface_capabilities.supportedCompositeAlpha &
       vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
      ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
    : (surface_capabilities.supportedCompositeAlpha &
       vk::CompositeAlphaFlagBitsKHR::eInherit)
      ? vk::CompositeAlphaFlagBitsKHR::eInherit
      : vk::CompositeAlphaFlagBitsKHR::eOpaque;

  // _swapchain_format = vk::Format::eB8G8R8A8Srgb; // ideal, we have eB8G8R8A8Unorm
  _swapchain_format = vk::Format::eB8G8R8A8Unorm;
  vk::SwapchainCreateInfoKHR swapchain_create_info {
    .flags = {},
    .surface = _surface.get(),
    .minImageCount = Framebuffer::max_presentable_images,
    .imageFormat = _swapchain_format,
    .imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
    .imageExtent = _extent,
    .imageArrayLayers = 1,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
    .imageSharingMode = vk::SharingMode::eExclusive,
    .queueFamilyIndexCount = {},
    .pQueueFamilyIndices = {},
    .preTransform = pre_transform,
    .compositeAlpha = composite_alpha,
    .presentMode = present_mode,
    .clipped = true,
    .oldSwapchain = nullptr};

  uint32_t indeces[1] {static_cast<uint32_t>(universal_queue_family_id)};
  swapchain_create_info.queueFamilyIndexCount = 1;
  swapchain_create_info.pQueueFamilyIndices = indeces;

  _swapchain =
    state.device().createSwapchainKHRUnique(swapchain_create_info).value;

  create_depthbuffer(_state);
  create_render_pass(_state);
  create_framebuffers(_state);

  _image_available_semaphore = _state.device().createSemaphore({}).value;
  _render_rinished_semaphore = _state.device().createSemaphore({}).value;
  _image_fence = _state.device()
                   .createFence({.flags = vk::FenceCreateFlagBits::eSignaled})
                   .value;
}

void mr::WindowContext::create_depthbuffer(const VulkanState &state)
{
  auto format = Image::find_supported_format(
    state,
    {vk::Format::eD32Sfloat,
     vk::Format::eD32SfloatS8Uint,
     vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment);
  _depthbuffer = Image(state,
                       _extent.width,
                       _extent.height,
                       format,
                       vk::ImageUsageFlagBits::eDepthStencilAttachment,
                       vk::ImageAspectFlagBits::eDepth);
  _depthbuffer.switch_layout(state,
                             vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

void mr::WindowContext::create_render_pass(const VulkanState &state)
{
  for (unsigned i = 0; i < gbuffers_number; i++) {
    _gbuffers[i] = Image(state,
                         _extent.width,
                         _extent.height,
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

void mr::WindowContext::create_framebuffers(const VulkanState &state)
{
  auto swampchain_images =
    state.device().getSwapchainImagesKHR(_swapchain.get()).value;

  for (uint i = 0; i < Framebuffer::max_presentable_images; i++) {
    _framebuffers[i] = Framebuffer(state,
                                   _render_pass.get(),
                                   _extent.width,
                                   _extent.height,
                                   _swapchain_format,
                                   swampchain_images[i],
                                   _gbuffers,
                                   _depthbuffer);
  }
}

void mr::WindowContext::render()
{
  static Shader _shader = Shader(_state, "default");
  const std::vector<vk::VertexInputAttributeDescription> descrs {
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
  static GraphicsPipeline pipeline = GraphicsPipeline(
    _state, _render_pass.get(), 0, &_shader, descrs, {bindings});

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

  static UniformBuffer uniform_buffer = UniformBuffer(_state, std::span{matr});
  static Texture texture = Texture(_state, "bin/textures/cat.png");
  // std::vector<DescriptorAttachment> attach {
  //   {.texture = &texture}, {.uniform_buffer = &uniiform_buffer}};
  std::vector<Descriptor::Attachment::Data> attach {
    {&texture}, {&uniform_buffer}
  };
  static Descriptor set = Descriptor(_state, &pipeline, attach);

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
  static const VertexBuffer vertex_buffer = VertexBuffer(_state, std::span{vertexes});
  static const IndexBuffer index_buffer = IndexBuffer(_state, std::span{indexes});

  /// light
  const std::vector<float> light_vertexes {-1, -1, 1, -1, 1, 1, -1, 1};
  const std::vector light_indexes {0, 1, 2, 2, 3, 0};
  static const VertexBuffer light_vertex_buffer = VertexBuffer(_state, std::span{light_vertexes});
  static const IndexBuffer light_index_buffer = IndexBuffer(_state, std::span{light_indexes});
  vk::VertexInputAttributeDescription light_descr {.location = 0,
                                                   .binding = 0,
                                                   .format =
                                                     vk::Format::eR32G32Sfloat,
                                                   .offset = 0};
  static Shader light_shader = Shader(_state, "light");
  std::vector<vk::DescriptorSetLayoutBinding> light_bindings(
    gbuffers_number,
    {
      .descriptorType = vk::DescriptorType::eInputAttachment,
      .descriptorCount = 1,
      .stageFlags = vk::ShaderStageFlagBits::eFragment,
    });
  for (unsigned i = 0; i < gbuffers_number; i++) {
    light_bindings[i].binding = i;
  }
  static GraphicsPipeline light_pipeline = GraphicsPipeline(_state,
                                                            _render_pass.get(),
                                                            1,
                                                            &light_shader,
                                                            {light_descr},
                                                            {light_bindings});
  std::vector<Descriptor::Attachment::Data> light_attach(gbuffers_number);
  for (unsigned i = 0; i < gbuffers_number; i++) {
    light_attach[i].emplace<Image *>(&_gbuffers[i]);
  }
  static Descriptor light_set =
    Descriptor(_state, &light_pipeline, light_attach);

  _state.device().waitForFences(_image_fence, VK_TRUE, UINT64_MAX);
  _state.device().resetFences(_image_fence);
  mr::uint image_index = 0;

  _state.device().acquireNextImageKHR(_swapchain.get(),
                                      UINT64_MAX,
                                      _image_available_semaphore,
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
    vk::PipelineBindPoint::eGraphics, pipeline.layout(), 0, {set.set()}, {});
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

  vk::Semaphore wait_semaphores[] = {_image_available_semaphore};
  vk::PipelineStageFlags wait_stages[] = {
    vk::PipelineStageFlagBits::eColorAttachmentOutput};
  vk::Semaphore signal_semaphores[] = {_render_rinished_semaphore};

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

  _state.queue().submit(submit_info, _image_fence);

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
