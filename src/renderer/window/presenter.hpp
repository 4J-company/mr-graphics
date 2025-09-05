#ifndef __MR_PRESENTER_HPP_
#define __MR_PRESENTER_HPP_

#include "pch.hpp"
#include "swapchain.hpp"
#include "input_state.hpp"
#include "resources/images/image.hpp"

namespace mr {
  // forward declaration of RenderContext class
  class RenderContext;

  class Presenter {
  private:
    // TODO(dk6): a lot of variables of derived classes can be here

    // Extent _extent;
    // beman::inplace_vector<ColorAttachmentImage, images_number> _images;

    // uint32_t _image_index = 0;
    // // TODO(dk6): i think here prev index is unnecessary
    // uint32_t _prev_image_index = 0;

    // // semaphores for waiting swapchain image is ready before light pass
    // beman::inplace_vector<vk::UniqueSemaphore, images_number> _image_available_semaphore;

    // // semaphores for waiting frame is ready before presentin
    // beman::inplace_vector<vk::UniqueSemaphore, images_number> _render_finished_semaphore;

    // const RenderContext *_parent = nullptr;

    // InputState _input_state;

  protected:
    // Presenter(const RenderContext &parent, Extent extent = {800, 600});
    Presenter() = default;

    Presenter(Presenter &&other) noexcept = default;
    Presenter &operator=(Presenter &&other) noexcept = default;

    virtual ~Presenter() = default;

  public:
    // Extent extent() const noexcept { return _extent; }

    // const RenderContext & render_context() const noexcept { return *_parent; }

    // Return rendering attachment info with target image
    virtual vk::RenderingAttachmentInfoKHR get_target_image() noexcept = 0;
    virtual void present() noexcept = 0;

    // TODO(dk6): we can remove here virtual
    // Pass this semaphore to render pass wait semaphores witch write in image
    virtual vk::Semaphore image_ready_semaphore() noexcept = 0;
    // Pass this semaphore to render pass signal semaphore witch write in image
    virtual vk::Semaphore render_finished_semaphore() noexcept = 0;

    // const InputState & input_state() const noexcept { return _input_state; }
    virtual void update_state() noexcept = 0;
  };
} // namespace mr
#endif // __MR_PRESENTER_HPP_
