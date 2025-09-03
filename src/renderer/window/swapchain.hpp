#ifndef __MR_WINDOW_SWAPCHAIN_HPP_
#define __MR_WINDOW_SWAPCHAIN_HPP_

namespace mr {
  struct Swapchain {
    const VulkanState *_state;
    vk::Format _format{vk::Format::eB8G8R8A8Unorm};
    vk::UniqueSwapchainKHR _swapchain;
    std::vector<SwapchainImage> _images;

    Swapchain(Swapchain &&) noexcept = default;
    Swapchain& operator=(Swapchain &&) noexcept = default;

    Swapchain(const VulkanState &state, vk::SurfaceKHR surface, Extent extent);

    void recreate(vk::SurfaceKHR surface);

    vk::Format format() const noexcept;
  };
}

#endif // __MR_WINDOW_SWAPCHAIN_HPP_
