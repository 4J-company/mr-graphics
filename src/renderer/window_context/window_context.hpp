#if !defined(wnd_ctx)
#define wnd_ctx

#include "pch.hpp"
#include "window.hpp"

namespace ter
{
  class window_context
  {
  private:
    vk::SwapchainKHR _swapchain;
    vk::SurfaceKHR _surface;

  public:
    void resize( size_t width, size_t height );
  };
}

#endif

