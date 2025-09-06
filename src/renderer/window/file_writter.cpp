#include "file_writer.hpp"
#include "render_context.hpp"
#include "swapchain.hpp"

// #define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

mr::FileWriter::FileWriter(const RenderContext &parent, Extent extent) : Presenter(parent, extent)
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

  auto &image = _images[_prev_image_index];
  image.switch_layout(_parent->vulkan_state(), vk::ImageLayout::eColorAttachmentOptimal);

  _current_image_avaible_semaphore = _image_available_semaphore[_prev_image_index].get();
  _current_render_finished_semaphore = _render_finished_semaphore[_prev_image_index].get();

  return image.attachment_info();
}

void mr::FileWriter::present() noexcept
{
  ASSERT(_parent != nullptr);

  // TODO(dk6): implement this. Note, we must wait render_finished_semaphore

  // TODO(dk6): tmp solution, this must be in image
  auto &image = _images[_prev_image_index];
  const auto &state = _parent->vulkan_state();

  image.switch_layout(state, vk::ImageLayout::eTransferSrcOptimal);

  size_t image_size = image.size() * 4; // TODO(dk6): image lies about it byte size
  auto stage_buffer = HostBuffer(state, image_size,
    vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);

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
  std::array<vk::PipelineStageFlags, 1> wait_stages {vk::PipelineStageFlagBits::eColorAttachmentOutput};

  auto [bufs, bufs_number] = command_unit.submit_info();
  vk::SubmitInfo submit_info {
    .waitSemaphoreCount = wait_sems.size(),
    .pWaitSemaphores = wait_sems.data(),
    .pWaitDstStageMask = wait_stages.data(),
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
  ASSERT(image._extent.width * image._extent.height * 4 == stage_buffer._size);
  stbi_write_png("frame.png", image._extent.width, image._extent.height, 4, data, image._extent.width * 4);
  stbi_write_tga("frame.tga", image._extent.width, image._extent.height, 4, data);
  stbi_write_bmp("frame.bmp", image._extent.width, image._extent.height, 4, data);

  char *data4comp = (char *)data;
  std::vector<char> in_rgb(image._extent.width * image._extent.height * 3);
  for (int i = 0; i < image._extent.width * image._extent.height; i++) {
    int j = i * 4;
    int k = i * 3;
    in_rgb[k] = data4comp[j + 2];
    in_rgb[k + 1] = data4comp[j + 1];
    in_rgb[k + 2] = data4comp[j];
  }
  stbi_write_png("frame_rgb.png", image._extent.width, image._extent.height, 3, in_rgb.data(), image._extent.width * 3);
  stbi_write_tga("frame_rgb.tga", image._extent.width, image._extent.height, 3, in_rgb.data());
  stbi_write_bmp("frame_rgb.bmp", image._extent.width, image._extent.height, 3, in_rgb.data());

  state.device().unmapMemory(stage_buffer._memory.get());
}

void mr::FileWriter::update_state() noexcept
{
  ASSERT(_parent != nullptr);
}