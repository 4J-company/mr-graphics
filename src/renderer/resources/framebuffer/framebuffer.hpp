#if !defined(__framebuffer_hpp_)
#define __framebuffer_hpp_

#include "pch.hpp"
#include "resources/images/image.hpp"

namespace ter
{
  struct Viewport
  {
    vk::Viewport viewport {};
    vk::Rect2D scissors {};
  };

  class WindowContext;

  class Framebuffer
  {
    friend class WindowContext;

  private:
    inline static const size_t max_presentable_images = 3, max_gbuffers = 1;

    vk::Framebuffer _framebuffer;

    uint _width, _height;
    Image _swapchain_image;
    std::array<Image, max_gbuffers> _gbuffers;

    Viewport _viewport;

  public:
    Framebuffer() = default;
    ~Framebuffer() = default;

    Framebuffer(VulkanApplication &va, uint width, uint height, vk::Format swapchain_format, vk::Image image);

    Framebuffer(Framebuffer &&other) noexcept = default;
    Framebuffer &operator=(Framebuffer &&other) noexcept = default;

    void resize(size_t width, size_t height);

    void set_viewport(vk::CommandBuffer cmd_buffer) const;

    const vk::Framebuffer & get_framebuffer() const { return _framebuffer; }
  };
} // namespace ter

#endif // __framebuffer_hpp_
