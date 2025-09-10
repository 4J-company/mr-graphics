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
    _images.emplace_back(_parent->vulkan_state(), _extent, Swapchain::default_format);
    _render_finished_semaphore.emplace_back(_parent->vulkan_state().device().createSemaphoreUnique({}).value);
    _image_available_semaphore.emplace_back(std::make_pair(
      _parent->vulkan_state().device().createSemaphoreUnique({}).value, true));
  }
}

vk::RenderingAttachmentInfoKHR mr::FileWriter::target_image_info() noexcept
{
  ASSERT(_parent != nullptr);

  _prev_image_index = _image_index;
  _image_index = (_image_index + 1) % images_number;

  auto &image = _images[_prev_image_index];
  image.switch_layout(_parent->vulkan_state(), vk::ImageLayout::eColorAttachmentOptimal);

  auto &[sem, first_usage] = _image_available_semaphore[_prev_image_index];
  _current_image_available_semaphore = first_usage ? VK_NULL_HANDLE : sem.get();
  first_usage = false;
  _current_render_finished_semaphore = _render_finished_semaphore[_prev_image_index].get();

  return image.attachment_info();
}

void mr::FileWriter::present() noexcept
{
  ASSERT(_parent != nullptr);

  // TODO(dk6): move all logic in separate thread

  // TODO(dk6): TMP solution, this must be in image, function like 'get_stage_buffer' or 'read'.
  //            But also it must use semaphores. Maybe pass command unit to image by argument,
  //            and set semaphores to command unit
  auto &image = _images[_prev_image_index];
  const auto &state = _parent->vulkan_state();

  image.switch_layout(state, vk::ImageLayout::eTransferSrcOptimal);

  printf("hello world\n");
  ASSERT(image._extent.width * image._extent.height * 4 == image.size());
  // size_t image_size = image.size() * 4; // TODO(dk6): image lies about it byte size
  size_t image_size = image.size();
  auto stage_buffer = HostBuffer(state, image_size, vk::BufferUsageFlagBits::eTransferDst);

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

  auto &command_unit = _parent->transfer_command_unit();
  command_unit.begin();
  command_unit->copyImageToBuffer(
    image._image.get(), image._layout, stage_buffer.buffer(), {region});
  command_unit.end();

  std::array wait_sems {_render_finished_semaphore[_prev_image_index].get()};
  std::array signal_sems {_image_available_semaphore[_prev_image_index].first.get()};
  std::array<vk::PipelineStageFlags, 1> wait_stages {vk::PipelineStageFlagBits::eColorAttachmentOutput};

  // TODO(dk6): i think we can add setting semaphores function to command_unit and
  //  get vk::SumbitInfo from sumbit_info()
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

  // TODO(dk6): add map/unmap methods to mr::Buffer
  void *data;
  state.device().mapMemory(stage_buffer._memory.get(), 0, stage_buffer._size, {}, &data);

  uint32_t height = image._extent.height;
  uint32_t width = image._extent.width;
  ASSERT(width * height * 4 == stage_buffer._size);

  char *data4comp = (char *)data;
  // convert BGRA -> RGBA
  for (uint32_t i = 0; i < stage_buffer._size; i += 4) {
    std::swap(data4comp[i], data4comp[i + 2]);
  }
  stbi_write_png(_frame_filename.c_str(), width, height, 4, data, width * 4);

  state.device().unmapMemory(stage_buffer._memory.get());
}

void mr::FileWriter::update_state() noexcept
{
  ASSERT(_parent != nullptr);
}

void mr::FileWriter::filename(const std::string_view filename) noexcept
{
  ASSERT(!filename.empty());
  // I think it faster - copy in already allocated memory if it has enough size
  //   instead allocate new memory in statement 'filename + ".png"' and move it
  _frame_filename = filename;
  _frame_filename += ".png";
}

