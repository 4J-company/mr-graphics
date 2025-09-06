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
  protected:
    Extent _extent;

    const RenderContext *_parent = nullptr;

    InputState _input_state;

    // Current semaphores used to decrease virtual calls number - derived classes will set them
    // semaphores for waiting swapchain image is ready before light pass
    vk::Semaphore _current_image_available_semaphore;
    // semaphores for waiting frame is ready before presentin
    vk::Semaphore _current_render_finished_semaphore;

  protected:
    Presenter(const RenderContext &parent, Extent extent = {800, 600})
      : _extent(extent), _parent(&parent) {}

    Presenter(Presenter &&other) noexcept = default;
    Presenter &operator=(Presenter &&other) noexcept = default;

    virtual ~Presenter() = default;

  public:
    // Return rendering attachment info with target image
    virtual vk::RenderingAttachmentInfoKHR target_image_info() noexcept = 0;
    virtual void present() noexcept = 0;

    virtual void update_state() noexcept = 0;

    // Pass this semaphore to render pass wait semaphores witch write in image
    // Note what it can be VK_NULL_HANDLE, for details look in FileWriter class
    vk::Semaphore image_available_semaphore() const noexcept { return _current_image_available_semaphore; }
    // Pass this semaphore to render pass signal semaphore witch write in image
    vk::Semaphore render_finished_semaphore() const noexcept { return _current_render_finished_semaphore; }

    Extent extent() const noexcept { return _extent; }
    const RenderContext & render_context() const noexcept { return *_parent; }
    const InputState & input_state() const noexcept { return _input_state; }
  };
} // namespace mr
#endif // __MR_PRESENTER_HPP_
