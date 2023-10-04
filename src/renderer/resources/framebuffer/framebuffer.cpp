#include "resources/framebuffer/framebuffer.hpp"

mr::Framebuffer::Framebuffer(const VulkanState &state, vk::RenderPass render_pass, uint width, uint height, vk::Format swapchain_format, 
  vk::Image final_target, Image &depthbuffer) : _width(width), _height(height)
{
  // assert(images.size() == max_presentable_images);
  _swapchain_images[0] = Image(state, width, height, swapchain_format, final_target);

  std::array attachments {_swapchain_images[0].image_view(), depthbuffer.image_view()};

  vk::FramebufferCreateInfo framebuffer_create_info {
      .renderPass = render_pass,
      .attachmentCount = static_cast<uint>(attachments.size()),
      .pAttachments = attachments.data(),
      .width = _width,
      .height = _height,
      .layers = 1,
  };

  _framebuffer = state.device().createFramebufferUnique(framebuffer_create_info).value;

  _viewport.viewport.x = 0.0f;
  _viewport.viewport.y = 0.0f;
  _viewport.viewport.width = static_cast<float>(width);
  _viewport.viewport.height = static_cast<float>(height);
  _viewport.viewport.minDepth = 0.0f;
  _viewport.viewport.maxDepth = 1.0f;

  _viewport.scissors.offset.setX(0).setY(0);
  _viewport.scissors.extent.setWidth(width).setHeight(height);
}
