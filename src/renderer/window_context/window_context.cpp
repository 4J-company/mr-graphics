#include "window_context.hpp"
#include "renderer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "resources/pipelines/graphics_pipeline.hpp"

ter::WindowContext::WindowContext(wnd::Window *parent, VulkanState state) : _parent(parent)
{
  _surface = xgfx::createSurface(_parent->xwindow_ptr(), state.instance());

  size_t universal_queue_family_id = 0;

  // Choose surface format
  const auto avaible_formats = state.phys_device().getSurfaceFormatsKHR(_surface).value;
  _swapchain_format = avaible_formats[0].format;
  for (const auto format : avaible_formats)
    if (format.format == vk::Format::eB8G8R8A8Unorm) // Prefered format
    {
      _swapchain_format = format.format;
      break;
    }
    else if (format.format == vk::Format::eR8G8B8A8Unorm)
      _swapchain_format = format.format;

  // Choose surface extent
  vk::SurfaceCapabilitiesKHR surface_capabilities;
  surface_capabilities = state.phys_device().getSurfaceCapabilitiesKHR(_surface).value;
  if (surface_capabilities.currentExtent.width == std::numeric_limits<uint>::max())
  {
    // If the surface size is undefined, the size is set to the size of the images requested.
    _extent.width = std::clamp(static_cast<uint32_t>(_parent->width()), surface_capabilities.minImageExtent.width,
                               surface_capabilities.maxImageExtent.width);
    _extent.height = std::clamp(static_cast<uint32_t>(_parent->height()), surface_capabilities.minImageExtent.height,
                                surface_capabilities.maxImageExtent.height);
  }
  else // If the surface size is defined, the swap chain size must match
    _extent = surface_capabilities.currentExtent;

  // Chosse surface present mode
  const auto avaible_present_modes = state.phys_device().getSurfacePresentModesKHR(_surface).value;
  const auto iter = std::find(avaible_present_modes.begin(), avaible_present_modes.end(), vk::PresentModeKHR::eMailbox);
  auto present_mode = (iter != avaible_present_modes.end())
      ? *iter
      : vk::PresentModeKHR::eFifo; // FIFO is required to be supported (by spec)

  // Choose surface transform
  vk::SurfaceTransformFlagBitsKHR pre_transform =
      (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
      ? vk::SurfaceTransformFlagBitsKHR::eIdentity
      : surface_capabilities.currentTransform;

  // Choose composite alpha
  vk::CompositeAlphaFlagBitsKHR composite_alpha =
      (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
      ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
      : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
      ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
      : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit)
      ? vk::CompositeAlphaFlagBitsKHR::eInherit
      : vk::CompositeAlphaFlagBitsKHR::eOpaque;

  vk::SwapchainCreateInfoKHR swapchain_create_info {.flags = {},
                                                    .surface = _surface,
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

  _swapchain = state.device().createSwapchainKHR(swapchain_create_info).value;
}

void ter::WindowContext::create_framebuffers(VulkanState state)
{
  std::vector<vk::Image> swampchain_images;
  swampchain_images = state.device().getSwapchainImagesKHR(_swapchain).value;

  for (uint i = 0; i < Framebuffer::max_presentable_images; i++)
    _framebuffers[i] = Framebuffer(state, _extent.width, _extent.height, _swapchain_format, swampchain_images[i]);
}

void ter::WindowContext::render()
{
  static CommandUnit command_unit {state};
  static vk::Semaphore _image_available_semaphore;
  static vk::Semaphore _render_rinished_semaphore;
  static vk::Fence _fence;
  static Shader _shader = Shader(state, "default");
  static GraphicsPipeline _pipeline = GraphicsPipeline(state, &_shader);

  state.device().waitForFences(_fence, VK_TRUE, UINT64_MAX);
  state.device().resetFences(_fence);

  ter::uint image_index = 0;

  state.device().acquireNextImageKHR(_swapchain, UINT64_MAX, _image_available_semaphore, nullptr, &image_index);

  command_unit.begin();

  vk::ClearValue clear_color;
  clear_color.setColor(vk::ClearColorValue(std::array<float, 4ULL>{0.f, 0.f, 0.f, 1.f})); // don't compile otherwise

  vk::RenderPassBeginInfo render_pass_info {
      .renderPass = state.render_pass(),
      .framebuffer = _framebuffers[image_index].framebuffer(),
      .renderArea = {{0, 0}, _extent},
      .clearValueCount = 1,
      .pClearValues = &clear_color,
  };

  command_unit->beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
  command_unit->bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline.pipeline());
  command_unit->setViewport(0, _framebuffers[image_index].viewport());
  command_unit->setScissor(0, _framebuffers[image_index].scissors());
  command_unit->draw(3, 1, 0, 0);

  command_unit.end();

  vk::Semaphore wait_semaphores[] = {_image_available_semaphore};
  vk::PipelineStageFlags wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
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

  state.queue().submit(submit_info, _fence);

  vk::SwapchainKHR swapchains[] = {_swapchain};
  vk::PresentInfoKHR present_info {
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = signal_semaphores,
      .swapchainCount = 1,
      .pSwapchains = swapchains,
      .pImageIndices = &image_index,
      .pResults = nullptr, // Optional
  };
  state.queue().presentKHR(present_info);
}
