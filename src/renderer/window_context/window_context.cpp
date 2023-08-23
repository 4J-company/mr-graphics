#include "window_context.hpp"

ter::WindowContext::WindowContext(window_system::Window *window, Application &app) : _window(window)
{
  /// REAL SHIT BELOW
  app._window_contexts.push_back(this);

  _surface = xgfx::createSurface(window->get_xwindow(), app.get_instance());

  size_t universal_queue_family_id = 0;

  // Choose surface format
  const auto avaible_formats = app._phys_device.getSurfaceFormatsKHR(_surface).value;
  _swapchain_format = avaible_formats[0].format;
  for (const auto format : avaible_formats)
    if (format.format == vk::Format::eR8G8B8A8Unorm) // Prefered format
    {
      _swapchain_format = format.format;
      break;
    }
    else if (format.format == vk::Format::eB8G8R8A8Unorm)
      _swapchain_format = format.format;

  // Choose surface extent
  vk::SurfaceCapabilitiesKHR surface_capabilities;
  surface_capabilities = app._phys_device.getSurfaceCapabilitiesKHR(_surface).value;
  if (surface_capabilities.currentExtent.width == std::numeric_limits<uint>::max())
  {
    // If the surface size is undefined, the size is set to the size of the images requested.
    _extent.width =
      std::clamp(static_cast<uint32_t>(window->_width), surface_capabilities.minImageExtent.width,
                 surface_capabilities.maxImageExtent.width);
    _extent.height =
      std::clamp(static_cast<uint32_t>(window->_height), surface_capabilities.minImageExtent.height,
                 surface_capabilities.maxImageExtent.height);
  }
  else // If the surface size is defined, the swap chain size must match
    _extent = surface_capabilities.currentExtent;

  // Chosse surface present mode
  const auto avaible_present_modes = app._phys_device.getSurfacePresentModesKHR(_surface).value;
  const auto iter =
    std::find(avaible_present_modes.begin(), avaible_present_modes.end(), vk::PresentModeKHR::eMailbox);
  auto present_mode =
    (iter != avaible_present_modes.end())
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

  vk::SwapchainCreateInfoKHR swapchain_create_info
  {
    .flags = {},
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
    .oldSwapchain = nullptr
  };

  uint32_t indeces[1] {static_cast<uint32_t>(universal_queue_family_id)};
  swapchain_create_info.queueFamilyIndexCount = 1;
  swapchain_create_info.pQueueFamilyIndices = indeces;

  _swapchain = app._device.createSwapchainKHR(swapchain_create_info).value;
}

void ter::WindowContext::create_framebuffers(Application &app)
{
  std::vector<vk::Image> swampchain_images;
  swampchain_images = app._device.getSwapchainImagesKHR(_swapchain).value;

  for (uint i = 0; i < Framebuffer::max_presentable_images; i++)
    _framebuffers[i] = Framebuffer(app, _extent.width, _extent.height, _swapchain_format, swampchain_images[i]);
}