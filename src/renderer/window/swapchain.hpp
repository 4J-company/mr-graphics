#ifndef __MR_WINDOW_SWAPCHAIN_HPP_
#define __MR_WINDOW_SWAPCHAIN_HPP_

namespace mr {
inline namespace graphics {
  // forward declaration of Window class
  class Window;

  class Swapchain {
    friend class Window;

  public:
    static inline constexpr int max_images_number = 8; // max teoretical swapchain images number

  private:
    const VulkanState *_state;
    vk::Format _format{vk::Format::eB8G8R8A8Unorm};
    vk::UniqueSwapchainKHR _swapchain;
    std::vector<SwapchainImage> _images;

  public:
    Swapchain(Swapchain &&) noexcept = default;
    Swapchain & operator=(Swapchain &&) noexcept = default;

    Swapchain(const VulkanState &state, vk::SurfaceKHR surface, Extent extent);

    void recreate(vk::SurfaceKHR surface);

    vk::Format format() const noexcept;
  };
}
}

#endif // __MR_WINDOW_SWAPCHAIN_HPP_
