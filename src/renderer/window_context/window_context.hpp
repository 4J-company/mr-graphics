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
  public:
    vk::SwapchainKHR _swapchain;
    vk::Format _swapchain_format;
    vk::SurfaceKHR _surface;
    vk::Extent2D _extent;

    std::array<Framebuffer, Framebuffer::max_presentable_images> _framebuffers;

    window_system::Window *_window;

  public:
    WindowContext(window_system::Window *window, Application &app);
    void create_framebuffers(Application &app);

    WindowContext() = default;
    ~WindowContext() = default;

    void resize(size_t width, size_t height);

    vk::Format get_swapchain_format() const { return _swapchain_format; }
  };
} // namespace ter
#endif // __window_context_hpp_
