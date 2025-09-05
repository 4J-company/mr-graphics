#include "file_writer.hpp"
#include "render_context.hpp"
#include "swapchain.hpp"

mr::FileWriter::FileWriter(const RenderContext &parent, Extent extent)
  : _extent(extent)
  , _parent(&parent)
{
  ASSERT(_parent != nullptr);

  for (uint32_t i = 0; i < images_number; i++) {
    // Same format as for swapchain
    _images.emplace_back(_parent->vulkan_state(), _extent, Swapchain::default_format());
    _render_finished_semaphore.emplace_back(_parent->vulkan_state().device().createSemaphoreUnique({}).value);
    _image_available_semaphore.emplace_back(_parent->vulkan_state().device().createSemaphoreUnique({}).value);
  }
}

vk::RenderingAttachmentInfoKHR mr::FileWriter::get_target_image() noexcept
{
  ASSERT(_parent != nullptr);

  _prev_image_index = _image_index;
  _image_index = (_image_index + 1) % images_number;

  return _images[_prev_image_index].attachment_info();
}

// #define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

void mr::FileWriter::present() noexcept
{
  ASSERT(_parent != nullptr);

  // TODO(dk6): implement this. Note, we must wait render_finished_semaphore

  // TODO(dk6): tmp solution, this must be in image
  auto &image = _images[_prev_image_index];
  const auto &state = _parent->vulkan_state();

  auto stage_buffer = HostBuffer(state, image.size(), vk::BufferUsageFlagBits::eTransferSrc);

  vk::ImageSubresourceLayers range {
    .aspectMask = image._aspect_flags,
    .mipLevel = image._mip_level - 1,
    .baseArrayLayer = 0,
    .layerCount = 1,
  };
  vk::BufferImageCopy region {
    .bufferOffset = 0,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource = range,
    .imageOffset = {0, 0, 0},
    .imageExtent = image._extent,
  };

  // TODO(dk6): use from render_context
  static CommandUnit command_unit(state);
  command_unit.begin();
  command_unit->copyImageToBuffer(
    image._image.get(), image._layout, stage_buffer.buffer(), {region});
  command_unit.end();

  std::array wait_sems {_render_finished_semaphore[_prev_image_index].get()};
  std::array signal_sems {_image_available_semaphore[_prev_image_index].get()};

  auto [bufs, bufs_number] = command_unit.submit_info();
  vk::SubmitInfo submit_info {
    .waitSemaphoreCount = wait_sems.size(),
    .pWaitSemaphores = wait_sems.data(),
    .commandBufferCount = bufs_number,
    .pCommandBuffers = bufs,
    .signalSemaphoreCount = signal_sems.size(),
    .pSignalSemaphores = signal_sems.data(),
  };
  auto fence = state.device().createFenceUnique({}).value;
  state.queue().submit(submit_info, fence.get());
  state.device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

  void *data;
  state.device().mapMemory(stage_buffer._memory.get(), 0, stage_buffer._size, {}, &data);
  ASSERT(image._extent.width * image._extent.height * 4 == image.size());
  stbi_write_png("file.png", image._extent.width, image._extent.height, 4, data, stage_buffer._size);
  state.device().unmapMemory(stage_buffer._memory.get());
}

vk::Semaphore mr::FileWriter::image_ready_semaphore() noexcept
{
  ASSERT(_parent != nullptr);
  return _image_available_semaphore[_prev_image_index].get();
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