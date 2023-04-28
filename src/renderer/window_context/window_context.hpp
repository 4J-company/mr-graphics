#if !defined(wnd_ctx)
#define wnd_ctx

#include "pch.hpp"
#include "window_system/window.hpp"

namespace ter
{
  class application;

  class window_context
  {
    friend class application;

  private:
    vk::SwapchainKHR _swapchain;
    vk::UniqueSurfaceKHR _surface;

    window_context( window_system::window *window, vk::Instance instance );

  public:
    window_context( void ) = default;

    void resize( size_t width, size_t height );
  };
}

#endif

