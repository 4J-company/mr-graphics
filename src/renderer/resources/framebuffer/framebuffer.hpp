#include "CrossWindow/XCB/XCBWindow.h"
#include "window_context.hpp"
#if !defined(__framebuffer_hpp_)
  #define __framebuffer_hpp_

  #include "pch.hpp"
  #include "resources/images/image.hpp"

namespace ter
{
  struct Viewport
  {
    vk::Viewport viewport;
    vk::Rect2D scissors;
  };

  class WindowContext;

  class Framebuffer
  {
    friend class WindowContext;

  private:
    inline static const size_t max_presentable_images = 3, max_gbuffers = 1;

    vk::Framebuffer _framebuffer;

    std::array<Image, max_presentable_images> _swapchain_images;
    std::array<Image, max_gbuffers> _gbuffers;

    Viewport _viewport;

  public:
    Framebuffer() = default;
    ~Framebuffer() = default;

    Framebuffer(Framebuffer &&other) noexcept = default;
    Framebuffer &operator=(Framebuffer &&other) noexcept = default;

    void resize(size_t width, size_t height);
  };
} // namespace ter

#endif // __framebuffer_hpp_
