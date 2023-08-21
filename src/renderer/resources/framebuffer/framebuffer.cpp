#include "resources/framebuffer/framebuffer.hpp"

ter::Framebuffer::Framebuffer(VulkanApplication &va, uint width, uint height, vk::Format swapchain_format, vk::Image image)
  : _width(width), _height(height)
{
  // assert(images.size() == max_presentable_images);
  _swapchain_image = Image(va, width, height, swapchain_format, image);

  vk::FramebufferCreateInfo framebuffer_create_info
  {
    .renderPass = va.get_render_pass(),
    .attachmentCount = 1,
    .pAttachments = &_swapchain_image.get_image_view(),
    .width = _width,
    .height = _height,
    .layers = 1,
  };

  vk::Result result;
  std::tie(result, _framebuffer) = va.get_device().createFramebuffer(framebuffer_create_info);
  assert(result == vk::Result::eSuccess);

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

void ter::Framebuffer::set_viewport(vk::CommandBuffer cmd_buffer) const
{
  cmd_buffer.setViewport(0, _viewport.viewport);
  cmd_buffer.setScissor(0, _viewport.scissors);
}
