#if !defined(wnd_ctx)
#define wnd_ctx

#include "pch.hpp"
#include "window_system/window.hpp"

namespace ter {
class application;

class window_context {
  friend class application;

private:
  inline static const uint32_t num_of_sc_images = 3;

  vk::SwapchainKHR _swapchain;
  vk::UniqueSurfaceKHR _surface;
  std::array<vk::Image, num_of_sc_images> _sc_images;

  window_context(window_system::window *window, const application &app);

public:
  window_context(void) = default;

  void resize(size_t width, size_t height);
};
} // namespace ter

#endif
