module;
#include "pch.hpp"
export module Framebuffer;

import Image;

export namespace mr {
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

      Framebuffer(const VulkanState &state, vk::RenderPass render_pass,
                  uint width, uint height, vk::Format swapchain_format,
                  vk::Image final_target,
                  std::array<Image, 6 /* constant... */> &gbuffers,
                  Image &depthbuffer);

      void resize(size_t width, size_t height);

      const vk::Framebuffer framebuffer() const { return _framebuffer.get(); }

      vk::Viewport viewport() const { return _viewport.viewport; }

      vk::Rect2D scissors() const { return _viewport.scissors; }
  };
} // namespace mr

mr::Framebuffer::Framebuffer(const VulkanState &state,
                             vk::RenderPass render_pass, uint width,
                             uint height, vk::Format swapchain_format,
                             vk::Image final_target,
                             std::array<Image, 6 /* constant... */> &gbuffers,
                             Image &depthbuffer)
    : _width(width)
    , _height(height)
{
  // assert(images.size() == max_presentable_images);
  _swapchain_images[0] =
    Image(state, width, height, swapchain_format, final_target);

  std::array<vk::ImageView, 6 + 2> attachments;
  attachments[0] = _swapchain_images[0].image_view();
  for (int i = 0; i < 6; i++) { attachments[i + 1] = gbuffers[i].image_view(); }
  attachments.back() = depthbuffer.image_view();

  vk::FramebufferCreateInfo framebuffer_create_info {
    .renderPass = render_pass,
    .attachmentCount = static_cast<uint>(attachments.size()),
    .pAttachments = attachments.data(),
    .width = _width,
    .height = _height,
    .layers = 1,
  };

  _framebuffer =
    state.device().createFramebufferUnique(framebuffer_create_info).value;

  _viewport.viewport.x = 0.0f;
  _viewport.viewport.y = 0.0f;
  _viewport.viewport.width = static_cast<float>(width);
  _viewport.viewport.height = static_cast<float>(height);
  _viewport.viewport.minDepth = 0.0f;
  _viewport.viewport.maxDepth = 1.0f;

  _viewport.scissors.offset.setX(0).setY(0);
  _viewport.scissors.extent.setWidth(width).setHeight(height);
}
