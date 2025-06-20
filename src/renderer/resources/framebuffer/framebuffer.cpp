#include "resources/framebuffer/framebuffer.hpp"
#include "resources/images/image.hpp"

mr::Framebuffer::Framebuffer(const VulkanState &state,
                             vk::RenderPass render_pass,
                             Extent extent,
                             SwapchainImage final_target,
                             std::array<ColorAttachmentImage, max_gbuffers> &gbuffers,
                             DepthImage &depthbuffer)
  : _extent{extent}
  , _swapchain_images{[&final_target] {
      std::array<SwapchainImage, max_presentable_images> tmp {std::move(final_target)};
      return tmp;
    }()}
  , _gbuffers{std::move(gbuffers)}
{
  std::array<vk::ImageView, 6 + 2> attachments;
  attachments[0] = _swapchain_images[0].image_view();
  for (int i = 0; i < 6; i++) { attachments[i + 1] = _gbuffers[i].image_view(); }
  attachments.back() = depthbuffer.image_view();

  vk::FramebufferCreateInfo framebuffer_create_info {
    .renderPass = render_pass,
    .attachmentCount = static_cast<uint>(attachments.size()),
    .pAttachments = attachments.data(),
    .width = _extent.width,
    .height = _extent.height,
    .layers = 1,
  };

  _framebuffer =
    state.device().createFramebufferUnique(framebuffer_create_info).value;

  _viewport.viewport.x = 0.0f;
  _viewport.viewport.y = 0.0f;
  _viewport.viewport.width = static_cast<float>(_extent.width);
  _viewport.viewport.height = static_cast<float>(_extent.height);
  _viewport.viewport.minDepth = 0.0f;
  _viewport.viewport.maxDepth = 1.0f;

  _viewport.scissors.offset.setX(0).setY(0);
  _viewport.scissors.extent.setWidth(_extent.width).setHeight(_extent.height);
}
