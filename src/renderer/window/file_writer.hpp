#ifndef __MR_FILE_WRITER_HPP_
#define __MR_FILE_WRITER_HPP_

#include "pch.hpp"
#include "swapchain.hpp"
#include "input_state.hpp"
#include "resources/images/image.hpp"

namespace mr {
  // forward declaration of Scene class
  class Scene;

  // forward declaration of RenderContext class
  class RenderContext;

  class FileWriter : public ResourceBase<FileWriter> {
  public:
    constexpr static uint32_t images_number = 3;
  private:
    Extent _extent;
    beman::inplace_vector<ColorAttachmentImage, images_number> _images;

    uint32_t _image_index = 0;
    // TODO(dk6): i think here prev index is unnecessary
    uint32_t _prev_image_index = 0;

    // semaphores for waiting swapchain image is ready before light pass
    beman::inplace_vector<vk::UniqueSemaphore, images_number> _image_available_semaphore;

    // semaphores for waiting frame is ready before presentin
    beman::inplace_vector<vk::UniqueSemaphore, images_number> _render_finished_semaphore;

    const RenderContext *_parent = nullptr;

    InputState _input_state;

  public:
    FileWriter(const RenderContext &parent, Extent extent = {800, 600});

    FileWriter(FileWriter &&other) noexcept = default;
    FileWriter &operator=(FileWriter &&other) noexcept = default;
    ~FileWriter() = default;

    Extent extent() const noexcept { return _extent; }

    const RenderContext & render_context() const noexcept { return *_parent; }

    // Return rendering attachment info with target image
    vk::RenderingAttachmentInfoKHR get_target_image() noexcept;
    void present() noexcept;

    // Pass this semaphore to render pass wait semaphores witch write in image
    vk::Semaphore image_ready_semaphore() noexcept;
    // Pass this semaphore to render pass signal semaphore witch write in image
    vk::Semaphore render_finished_semaphore() noexcept;

    const InputState & input_state() const noexcept { return _input_state; }
    void update_state() noexcept;
  };

  MR_DECLARE_HANDLE(FileWriter);
} // namespace mr
#endif // __MR_FILE_WRITER_HPP_
