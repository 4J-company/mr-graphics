#ifndef __MR_FRAMEBUFFER_HPP_
#define __MR_FRAMEBUFFER_HPP_

#include "pch.hpp"
#include "resources/images/image.hpp"

namespace mr {
  struct Viewport {
      vk::Viewport viewport {};
      vk::Rect2D scissors {};
  };

  class RenderContext;

  class Framebuffer {
      friend class RenderContext;

    private:
      static inline const size_t max_presentable_images = 2, max_gbuffers = 1;

      vk::UniqueFramebuffer _framebuffer;

      Extent _extent;
      std::array<Image, max_presentable_images> _swapchain_images;
      std::array<Image, max_gbuffers> _gbuffers;

      Viewport _viewport;

    public:
      Framebuffer() = default;

      Framebuffer(const VulkanState &state, vk::RenderPass render_pass,
                  Extent extent,
                  Image final_target,
                  std::array<Image, 6 /* constant... */> &gbuffers,
                  Image &depthbuffer);

      void resize(size_t width, size_t height);

      const vk::Framebuffer framebuffer() const { return _framebuffer.get(); }

      vk::Viewport viewport() const { return _viewport.viewport; }

      vk::Rect2D scissors() const { return _viewport.scissors; }
  };
} // namespace mr

#endif // __MR_FRAMEBUFFER_HPP_
