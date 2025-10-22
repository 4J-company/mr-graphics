#ifndef __MR_WINDOW_SWAPCHAIN_HPP_
#define __MR_WINDOW_SWAPCHAIN_HPP_

#include "resources/images/image.hpp"

namespace mr {
inline namespace graphics {
  // forward declaration of Window class
  class Window;

  class Swapchain {
    friend class Window;

  public:
    static inline constexpr int max_images_number = 8; // max teoretical swapchain images number
    static inline constexpr vk::Format default_format = vk::Format::eB8G8R8A8Unorm;

    const VulkanState *_state;
    vk::Format _format = default_format;
    vkb::Swapchain _swapchain;
    SmallVector<SwapchainImage, 4> _images;

  public:
    Swapchain(Swapchain &&) noexcept = default;
    Swapchain & operator=(Swapchain &&) noexcept = default;

    Swapchain(const VulkanState &state, vk::SurfaceKHR surface, Extent extent);

    operator vk::SwapchainKHR() noexcept {
      return _swapchain.swapchain;
    }

    vk::Format format() const noexcept;
  };
}
}

#endif // __MR_WINDOW_SWAPCHAIN_HPP_
