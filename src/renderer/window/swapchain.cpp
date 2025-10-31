#include "swapchain.hpp"

mr::Swapchain::Swapchain(const VulkanState &state, vk::SurfaceKHR surface, Extent extent)
  : _state(&state)
{
  vkb::SwapchainBuilder builder{state.phys_device(), state.device(), surface};
  vkb::Result<vkb::Swapchain> swapchain = builder
    .set_desired_format({static_cast<VkFormat>(_format), VK_COLORSPACE_SRGB_NONLINEAR_KHR})
    .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
    .set_desired_extent(extent.width, extent.height)
    .build();

  if (not swapchain) {
    MR_ERROR("Cannot create VkSwapchainKHR. {}\n", swapchain.error().message());
    std::exit(1);
  }
  _swapchain = swapchain.value();

  std::vector<VkImage> images = swapchain.value().get_images().value();
  std::vector<VkImageView> image_views = swapchain.value().get_image_views().value();

  _images.reserve(images.size());
  for (int i = 0; i < images.size(); i++) {
    _images.emplace_back(state, extent, _format, images[i], image_views[i]);
  }
}

vk::Format mr::Swapchain::format() const noexcept { return _format; }
