#include "window_context.hpp"

ter::window_context::window_context(window_system::window *window,
  const application &app)
{
  vk::Instance inst;
  VkSurfaceKHR surface_handle = xgfx::createSurface(window->get_xwindow(), inst);
  
  _surface = vk::UniqueSurfaceKHR(surface_handle);
  
  size_t universal_queue_family_id = 0;
  
  std::vector<vk::SurfaceFormatKHR> formats =
    app._phys_device.getSurfaceFormatsKHR(surface_handle);
  assert(!formats.empty());
  _swapchain_format = (formats[0].format == vk::Format::eUndefined)
    ? vk::Format::eB8G8R8A8Unorm : formats[0].format;
  
  vk::SurfaceCapabilitiesKHR surface_capabilities =
    app._phys_device.getSurfaceCapabilitiesKHR(surface_handle);
  vk::Extent2D swapchain_extent;
  if (surface_capabilities.currentExtent.width ==
    std::numeric_limits<uint32_t>::max())
  {
    // If the surface size is undefined, the size is set to the size of the
    // images requested.
    swapchain_extent.width =
      std::clamp(static_cast<uint32_t>(window->_width),
        surface_capabilities.minImageExtent.width,
        surface_capabilities.maxImageExtent.width);
    swapchain_extent.height =
      std::clamp(static_cast<uint32_t>(window->_height),
        surface_capabilities.minImageExtent.height,
        surface_capabilities.maxImageExtent.height);
  }
  else // If the surface size is defined, the swap chain size must match
    swapchain_extent = surface_capabilities.currentExtent;
  
  // The FIFO present mode is guaranteed by the spec to be supported
  vk::PresentModeKHR swapchain_present_mode = vk::PresentModeKHR::eMailbox;
  
  vk::SurfaceTransformFlagBitsKHR pre_transform =
    (surface_capabilities.supportedTransforms &
      vk::SurfaceTransformFlagBitsKHR::eIdentity)
      ? vk::SurfaceTransformFlagBitsKHR::eIdentity
      : surface_capabilities.currentTransform;
  
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
  
  vk::SwapchainCreateInfoKHR swapchain_create_info
  {
    .flags                 = {},
    .surface               = surface_handle,
    .minImageCount         = framebuffer::num_of_presentable_images,
    .imageFormat           = _swapchain_format,
    .imageColorSpace       = vk::ColorSpaceKHR::eSrgbNonlinear,
    .imageExtent           = swapchain_extent,
    .imageArrayLayers      = 1,
    .imageUsage            = vk::ImageUsageFlagBits::eColorAttachment,
    .imageSharingMode      = vk::SharingMode::eExclusive,
    .queueFamilyIndexCount = {},
    .pQueueFamilyIndices   = {},
    .preTransform          = pre_transform,
    .compositeAlpha        = composite_alpha,
    .presentMode           = swapchain_present_mode,
    .clipped               = true,
    .oldSwapchain          = nullptr
  };
  
  uint32_t indeces[1] { static_cast<uint32_t>(universal_queue_family_id) };
  
  swapchain_create_info.queueFamilyIndexCount = 1;
  swapchain_create_info.pQueueFamilyIndices   = indeces;
  
  _swapchain = app._device.createSwapchainKHR(swapchain_create_info);
  
  std::memcpy(_framebuffer._swapchain_images.data(),
    app._device.getSwapchainImagesKHR(_swapchain).data(),
    framebuffer::num_of_presentable_images);
}

ter::framebuffer::framebuffer(const vk::Format &image_format, const vk::Device &device)
{
  vk::ImageSubresourceRange image_subresourse_range 
  {
    .aspectMask     = vk::ImageAspectFlagBits::eColor,
    .baseMipLevel   = 0,
    .levelCount     = 1,
    .baseArrayLayer = 0,
    .layerCount     = 1
  };

  vk::ImageViewCreateInfo imageViewCreateInfo
  {
    .flags            = {},
    .image            = {},
    .viewType         = vk::ImageViewType::e2D,
    .format           = image_format,
    .components       = {},
    .subresourceRange = image_subresourse_range
  };

  std::array<int, framebuffer::num_of_presentable_images> indexes; // indexes of images passed
  std::iota(indexes.begin(), indexes.end(), framebuffer::num_of_presentable_images); // filling indexes
  
  std::for_each(std::execution::par, indexes.begin(), indexes.end(),
    [=, &imageViewCreateInfo](int index)
    {
      // create image view
      imageViewCreateInfo.image = _swapchain_images[index];
      _swapchain_images_views[index] =
        device.createImageView(imageViewCreateInfo);
    });
}

ter::framebuffer::~framebuffer()
{
}
