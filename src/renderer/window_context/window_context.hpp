#if !defined(__window_context_hpp_)
  #define __window_context_hpp_

  #include "pch.hpp"
  #include "resources/resources.hpp"
  #include "vulkan_application.hpp"

namespace wnd
{
  // forward declaration
  class Window;
} // namespace wnd

namespace ter
{
  class WindowContext
  {
  private:
    vk::SwapchainKHR _swapchain;
    vk::Format _swapchain_format;
    vk::SurfaceKHR _surface;
    vk::Extent2D _extent;
    std::array<Framebuffer, Framebuffer::max_presentable_images> _framebuffers;

    VulkanState state;

    wnd::Window *_parent;

  public:
    WindowContext() = default;
    WindowContext(wnd::Window *parent, VulkanState state);
    WindowContext(WindowContext &&other) noexcept = default;
    WindowContext &operator=(WindowContext &&other) noexcept = default;

    ~WindowContext() = default;

    void create_framebuffers(VulkanState state);
    void resize(size_t width, size_t height);
    void render();
  };
} // namespace ter
#endif // __window_context_hpp_
