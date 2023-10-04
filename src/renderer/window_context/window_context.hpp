#if !defined(__window_context_hpp_)
  #define __window_context_hpp_

  #include "pch.hpp"
  #include "resources/resources.hpp"
  #include "vulkan_application.hpp"

namespace mr
{
  class Window;

  class WindowContext
  {
  private:
    vk::UniqueSwapchainKHR _swapchain;
    vk::Format _swapchain_format;
    vk::UniqueSurfaceKHR _surface;
    vk::Extent2D _extent;
    std::array<Framebuffer, Framebuffer::max_presentable_images> _framebuffers;

    vk::UniqueRenderPass _render_pass;
    Image _depthbuffer;

    VulkanState _state;

    vk::Semaphore _image_available_semaphore;
    vk::Semaphore _render_rinished_semaphore;
    vk::Fence _image_fence;

    Window *_parent;

  public:
    WindowContext() = default;
    WindowContext(Window *parent, const VulkanState &state);
    WindowContext(WindowContext &&other) noexcept = default;
    WindowContext &operator=(WindowContext &&other) noexcept = default;

    ~WindowContext() = default;

    void create_framebuffers(const VulkanState &state);
    void create_depthbuffer(const VulkanState &state);
    void create_render_pass(const VulkanState &state);
    void resize(size_t width, size_t height);
    void render();
  };
} // namespace mr
#endif // __window_context_hpp_
