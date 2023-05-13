#include "window_context.hpp"

ter::window_context::window_context(window_system::window *window,
                                    const application &app) {
  VkSurfaceKHR surface_handle;
  glfwCreateWindowSurface(static_cast<VkInstance>(app._instance),
                          window->_window, nullptr, &surface_handle);
  _surface = vk::UniqueSurfaceKHR(surface_handle);

  size_t universal_queue_family_id = 0;

  std::vector<vk::SurfaceFormatKHR> formats =
      app._phys_device.getSurfaceFormatsKHR(surface_handle);
  assert(!formats.empty());
  _swapchain_format = (formats[0].format == vk::Format::eUndefined)
                          ? vk::Format::eB8G8R8A8Unorm
                          : formats[0].format;

  vk::SurfaceCapabilitiesKHR surfaceCapabilities =
      app._phys_device.getSurfaceCapabilitiesKHR(surface_handle);
  vk::Extent2D swapchainExtent;
  if (surfaceCapabilities.currentExtent.width ==
      std::numeric_limits<uint32_t>::max()) {
    // If the surface size is undefined, the size is set to the size of the
    // images requested.
    swapchainExtent.width =
        std::clamp(static_cast<uint32_t>(window->_width),
                   surfaceCapabilities.minImageExtent.width,
                   surfaceCapabilities.maxImageExtent.width);
    swapchainExtent.height =
        std::clamp(static_cast<uint32_t>(window->_height),
                   surfaceCapabilities.minImageExtent.height,
                   surfaceCapabilities.maxImageExtent.height);
  } else // If the surface size is defined, the swap chain size must match
    swapchainExtent = surfaceCapabilities.currentExtent;

  // The FIFO present mode is guaranteed by the spec to be supported
  vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eMailbox;

  vk::SurfaceTransformFlagBitsKHR preTransform =
      (surfaceCapabilities.supportedTransforms &
       vk::SurfaceTransformFlagBitsKHR::eIdentity)
          ? vk::SurfaceTransformFlagBitsKHR::eIdentity
          : surfaceCapabilities.currentTransform;

  vk::CompositeAlphaFlagBitsKHR compositeAlpha =
      (surfaceCapabilities.supportedCompositeAlpha &
       vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
          ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
      : (surfaceCapabilities.supportedCompositeAlpha &
         vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
          ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
      : (surfaceCapabilities.supportedCompositeAlpha &
         vk::CompositeAlphaFlagBitsKHR::eInherit)
          ? vk::CompositeAlphaFlagBitsKHR::eInherit
          : vk::CompositeAlphaFlagBitsKHR::eOpaque;

  vk::SwapchainCreateInfoKHR swapChainCreateInfo(
      vk::SwapchainCreateFlagsKHR(), surface_handle,
      framebuffer::num_of_presentable_images, _swapchain_format,
      vk::ColorSpaceKHR::eSrgbNonlinear, swapchainExtent, 1,
      vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive, {},
      preTransform, compositeAlpha, swapchainPresentMode, true, nullptr);

  uint32_t indeces[1]{static_cast<uint32_t>(universal_queue_family_id)};

  swapChainCreateInfo.queueFamilyIndexCount = 1;
  swapChainCreateInfo.pQueueFamilyIndices = indeces;

  _swapchain = app._device.createSwapchainKHR(swapChainCreateInfo);

  std::memcpy(_framebuffer._swapchain_images.data(),
              app._device.getSwapchainImagesKHR(_swapchain).data(),
              framebuffer::num_of_presentable_images);
}
ter::framebuffer::framebuffer(const vk::Format &image_format,
                              const vk::Device &device) {
  vk::ImageViewCreateInfo imageViewCreateInfo(
      {}, {}, vk::ImageViewType::e2D, image_format, {},
      {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
  std::array<int, framebuffer::num_of_presentable_images>
      indexes; // indexes of images passed
  std::iota(indexes.begin(), indexes.end(),
            framebuffer::num_of_presentable_images); // filling indexes

  std::for_each(std::execution::par, indexes.begin(), indexes.end(),
                [=, &imageViewCreateInfo](int index) {
                  // create image view
                  imageViewCreateInfo.image = _swapchain_images[index];
                  _swapchain_images_views[index] =
                      device.createImageView(imageViewCreateInfo);
                });
}

ter::framebuffer::~framebuffer() {}
