#ifndef __MR_FILE_WRITER_HPP_
#define __MR_FILE_WRITER_HPP_

#include "pch.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "swapchain.hpp"
#include "input_state.hpp"
#include "resources/images/image.hpp"
#include "presenter.hpp"

namespace mr {
inline namespace graphics {
  class FileWriter : public Presenter, public ResourceBase<FileWriter> {
  public:
    constexpr static uint32_t images_number = 3;
  private:
    beman::inplace_vector<ColorAttachmentImage, images_number> _images;

    uint32_t _image_index = 0;

    // bool is for check is semaphore usage first - we must skip first semaphore usage,
    // because at start semaphores are not signaled and we have no way to signal instead
    // dummy queue submit
    using ImageAvailableSemaphoreT = std::pair<vk::UniqueSemaphore, bool>;
    // semaphores for waiting swapchain image is ready before light pass
    beman::inplace_vector<ImageAvailableSemaphoreT, images_number> _image_available_semaphore;
    // semaphores for waiting frame is ready before presentin
    beman::inplace_vector<vk::UniqueSemaphore, images_number> _render_finished_semaphore;

    CommandUnit _images_command_unit;

    std::string _frame_filename = "frame";

  public:
    FileWriter(const RenderContext &parent, Extent extent = {800, 600});

    FileWriter(FileWriter &&other) noexcept = default;
    FileWriter &operator=(FileWriter &&other) noexcept = default;
    ~FileWriter() = default;

    // Return rendering attachment info with target image
    vk::RenderingAttachmentInfoKHR target_image_info() noexcept final;
    void present() noexcept final;

    // without extension! now by default we save in .png
    void filename(const std::string_view filename) noexcept;
  };

  MR_DECLARE_HANDLE(FileWriter);
}
} // namespace mr
#endif // __MR_FILE_WRITER_HPP_
