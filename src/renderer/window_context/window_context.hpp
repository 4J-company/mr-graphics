#if !defined(__window_context_hpp_)
  #define __window_context_hpp_

  #include "pch.hpp"
  #include "resources/resources.hpp"
  #include "window_system/window.hpp"

namespace ter
{
  class Application;

  class WindowContext
  {
    friend class Application;

  private:
    vk::SwapchainKHR _swapchain;
    vk::Format _swapchain_format;
    vk::UniqueSurfaceKHR _surface;

    Framebuffer _framebuffer;

  public:
    WindowContext(window_system::Window *window, const Application &app);

    WindowContext() = default;
    ~WindowContext() = default;

    void resize(size_t width, size_t height);
  };
} // namespace ter
#endif // __window_context_hpp_
