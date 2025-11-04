#include "dummy_presenter.hpp"
#include "swapchain.hpp"
#include "render_context.hpp"
#include <vulkan/vulkan_core.h>

mr::DummyPresenter::DummyPresenter(const RenderContext &parent, Extent extent) : Presenter(parent, extent)
{
  ASSERT(_parent != nullptr);

  for (uint32_t i = 0; i < images_number; i++) {
    // Same format as for swapchain
    _images.emplace_back(_parent->vulkan_state(), _extent, Swapchain::default_format);
    _images.back().switch_layout(vk::ImageLayout::eColorAttachmentOptimal);
  }
}

vk::RenderingAttachmentInfoKHR mr::DummyPresenter::target_image_info() noexcept
{
  ASSERT(_parent != nullptr);

  // We start with 1st image (not 0): 1 -> 2 -> 0 -> 1 -> ...
  _image_index = (_image_index + 1) % images_number;

  _current_image_available_semaphore = VK_NULL_HANDLE;
  _current_render_finished_semaphore = VK_NULL_HANDLE;

  return _images[_image_index].attachment_info();
}

void mr::DummyPresenter::present() noexcept
{
  ASSERT(_parent != nullptr);
  // Do nothing
}

