#include "file_writer.hpp"
#include "render_context.hpp"

mr::FileWriter::FileWriter(const RenderContext &parent, Extent extent)
  : _extent(extent)
  , _parent(&parent)
{
  ASSERT(_parent != nullptr);

  for (uint32_t i = 0; i < images_number; i++) {
    _images.emplace_back(_parent->vulkan_state(), _extent, vk::Format::eR32G32B32A32Sfloat);
  }
}

vk::RenderingAttachmentInfoKHR mr::FileWriter::get_target_image() noexcept
{
  ASSERT(_parent != nullptr);

  _prev_image_index = _image_index;
  _image_index = (_image_index + 1) % images_number;

  return _images[_prev_image_index].attachment_info();
}

void mr::FileWriter::present() noexcept
{
  ASSERT(_parent != nullptr);
  // TODO(dk6): implement this. Not, we must wait render_finished_semaphore
  _images[_prev_image_index].copy_to_host();
}

vk::Semaphore mr::FileWriter::image_ready_semaphore() noexcept
{
  ASSERT(_parent != nullptr);
  return VK_NULL_HANDLE;
}

vk::Semaphore mr::FileWriter::render_finished_semaphore() noexcept
{
  ASSERT(_parent != nullptr);
  return _render_finished_semaphore[_prev_image_index].get();
}

void mr::FileWriter::update_state() noexcept
{
  ASSERT(_parent != nullptr);
}