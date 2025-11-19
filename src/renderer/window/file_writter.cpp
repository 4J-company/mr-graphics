#include "file_writer.hpp"
#include "render_context.hpp"
#include "swapchain.hpp"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#undef STB_IMAGE_WRITE_IMPLEMENTATION

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

  // We start with 1st image (not 0): 1 -> 2 -> 0 -> 1 -> ...
  _image_index = (_image_index + 1) % images_number;

  auto &image = _images[_image_index];
  CommandUnit command_unit {_parent->vulkan_state()};
  command_unit.begin();
  image.switch_layout(command_unit, vk::ImageLayout::eColorAttachmentOptimal);
  command_unit.end();

  UniqueFenceGuard(_parent->vulkan_state().device(), command_unit.submit(_parent->vulkan_state()));

  auto &[sem, first_usage] = _image_available_semaphore[_image_index];
  _current_image_available_semaphore = first_usage ? VK_NULL_HANDLE : sem.get();
  first_usage = false;
  _current_render_finished_semaphore = _render_finished_semaphore[_image_index].get();

  return image.attachment_info();
}

void mr::FileWriter::present() noexcept
{
  ASSERT(_parent != nullptr);

  // TODO(dk6): move all logic in separate thread

  auto &image = _images[_image_index];
  const auto &state = _parent->vulkan_state();

  auto &command_unit = _parent->transfer_command_unit();
  command_unit.begin();

  command_unit.add_signal_semaphore(_image_available_semaphore[_image_index].first.get());
  command_unit.add_wait_semaphore(_render_finished_semaphore[_image_index].get(),
                                  vk::PipelineStageFlagBits::eColorAttachmentOutput);

  auto stage_buffer = image.read_to_host_buffer(command_unit);

  UniqueFenceGuard(_parent->vulkan_state().device(), command_unit.submit(_parent->vulkan_state()));

  auto data = stage_buffer.copy();
  ASSERT(data.size() % 4 == 0);

  // convert BGRA -> RGBA
  for (uint32_t i = 0; i < data.size(); i += 4) {
    std::swap(data[i], data[i + 2]);
  }

  uint32_t height = image.extent().height;
  uint32_t width = image.extent().width;
  stbi_write_png(_frame_filename.c_str(), width, height, 4, data.data(), width * 4);
}

void mr::FileWriter::filename(const std::string_view filename) noexcept
{
  ASSERT(!filename.empty());
  // I think it faster - copy in already allocated memory if it has enough size
  //   instead allocate new memory in statement 'filename + ".png"' and move it
  _frame_filename = filename;
  _frame_filename += ".png";
}

