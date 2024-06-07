#if !defined(__framebuffer_hpp_)
#define __framebuffer_hpp_

#include "pch.hpp"
#include "resources/images/image.hpp"

namespace mr {
  struct Viewport {
      vk::Viewport viewport {};
      vk::Rect2D scissors {};
  };

  class WindowContext;

  class Framebuffer {
      friend class WindowContext;

    private:
      static inline const size_t max_presentable_images = 2, max_gbuffers = 1;

      vk::UniqueFramebuffer _framebuffer;

      uint _width, _height;
      std::array<Image, max_presentable_images> _swapchain_images;
      std::array<Image, max_gbuffers> _gbuffers;

      Viewport _viewport;

    public:
      Framebuffer() = default;
      ~Framebuffer() = default;

      Framebuffer(const VulkanState &state, vk::RenderPass render_pass,
                  uint width, uint height, vk::Format swapchain_format,
                  vk::Image final_target,
                  std::array<Image, 6 /* constant... */> &gbuffers,
                  Image &depthbuffer);

      Framebuffer(Framebuffer &&other) noexcept = default;
      Framebuffer &operator=(Framebuffer &&other) noexcept = default;

      void resize(size_t width, size_t height);

      const vk::Framebuffer framebuffer() const { return _framebuffer.get(); }

      vk::Viewport viewport() const { return _viewport.viewport; }

      vk::Rect2D scissors() const { return _viewport.scissors; }
  };
} // namespace mr

#endif // __framebuffer_hpp_
