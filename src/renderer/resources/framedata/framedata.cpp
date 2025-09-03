#include "resources/framedata/framedata.hpp"
#include "resources/images/image.hpp"

mr::FrameData::FrameData(const VulkanState &state, Extent extent, const SwapchainImage &target)
  : _extent{extent}
  , _target{target}
  , _depthbuffer{state, extent}
{
  for (auto _ : std::views::iota(0u, max_gbuffers)) {
    _gbuffers.emplace_back(mr::ColorAttachmentImage(state, _extent, vk::Format::eR32G32B32A32Sfloat));
  }

  std::array<vk::ImageView, 6 + 2> attachments;
  attachments.front() = _target.image_view();
  for (int i = 0; i < 6; i++) {
    attachments[i + 1] = _gbuffers[i].image_view();
  }
  attachments.back() = _depthbuffer.image_view();

  _viewport.viewport.x = 0.0f;
  _viewport.viewport.y = 0.0f;
  _viewport.viewport.width = static_cast<float>(_extent.width);
  _viewport.viewport.height = static_cast<float>(_extent.height);
  _viewport.viewport.minDepth = 0.0f;
  _viewport.viewport.maxDepth = 1.0f;

  _viewport.scissors.offset.setX(0).setY(0);
  _viewport.scissors.extent.setWidth(_extent.width).setHeight(_extent.height);
}
