#include "resources/framebuffer/framebuffer.hpp"

ter::Framebuffer::Framebuffer(VulkanState state, uint width, uint height, vk::Format swapchain_format, vk::Image image)
    : _width(width), _height(height)
{
  // assert(images.size() == max_presentable_images);
  _swapchain_images[0] = Image(state, width, height, swapchain_format, image);

  vk::FramebufferCreateInfo framebuffer_create_info {
      .renderPass = state.render_pass(),
      .attachmentCount = 1,
      .pAttachments = &_swapchain_images[0].image_view(),
      .width = _width,
      .height = _height,
      .layers = 1,
  };

  _framebuffer = state.device().createFramebuffer(framebuffer_create_info).value;

  _viewport.viewport.x = 0.0f;
  _viewport.viewport.y = 0.0f;
  _viewport.viewport.width = static_cast<float>(width);
  // _viewport.viewport.height = -static_cast<float>(height);
  _viewport.viewport.height = static_cast<float>(height);
  _viewport.viewport.minDepth = 0.0f;
  _viewport.viewport.maxDepth = 1.0f;

  // memset(&_viewport.scissors.offset, 0, sizeof(vk::Offset2D));
  _viewport.scissors.offset.setX(0).setY(0);
  _viewport.scissors.extent.setWidth(width).setHeight(height);
}
