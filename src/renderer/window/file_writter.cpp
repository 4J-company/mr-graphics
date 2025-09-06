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
    _image_available_semaphore.emplace_back(_parent->vulkan_state().device().createSemaphoreUnique({}).value);
  }
}

vk::RenderingAttachmentInfoKHR mr::FileWriter::target_image_info() noexcept
{
  ASSERT(_parent != nullptr);

  _prev_image_index = _image_index;
  _image_index = (_image_index + 1) % images_number;

  auto &image = _images[_prev_image_index];
  image.switch_layout(_parent->vulkan_state(), vk::ImageLayout::eColorAttachmentOptimal);

  // TODO(dk6): We have vulkan sync issues, we must fix them.
  //            Now nullptr works because there is workaround for this
  //              in render context 'render' function
  _current_image_available_semaphore = nullptr;
  // _current_image_available_semaphore = _image_available_semaphore[_prev_image_index].get();
  _current_render_finished_semaphore = _render_finished_semaphore[_prev_image_index].get();

  return image.attachment_info();
}

void mr::FileWriter::present() noexcept
{
  ASSERT(_parent != nullptr);

  // TODO(dk6): TMP solution, this must be in image, function like 'get_stage_buffer'.
  //            But also it must use semaphores. Maybe pass command unit to image by argument,
  //            and set semaphores to command unit
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
  auto &command_unit = _parent->transfer_command_unit();
  command_unit.begin();
  command_unit->copyImageToBuffer(
    image._image.get(), image._layout, stage_buffer.buffer(), {region});
  command_unit.end();

  std::array wait_sems {_render_finished_semaphore[_prev_image_index].get()};
  std::array signal_sems {_image_available_semaphore[_prev_image_index].get()};
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
  // std::vector<char> data_rgb(width * height * 3);
  for (int i = 0; i < width * height; i++) {
    int j = i * 4;
    int k = i * 3;
    std::swap(data4comp[j], data4comp[j + 2]);
    // data_rgb[k] = data4comp[j];
    // data_rgb[k + 1] = data4comp[j + 1];
    // data_rgb[k + 2] = data4comp[j + 2];
  }

  stbi_write_png(_frame_filename.c_str(), width, height, 4, data, width * 4);
  // stbi_write_png("frame_rgb.png", width, height, 3, data_rgb.data(), width * 3);

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

