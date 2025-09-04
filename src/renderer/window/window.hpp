#ifndef __MR_WINDOW_HPP_
#define __MR_WINDOW_HPP_

#include "pch.hpp"
#include "swapchain.hpp"
#include "input_state.hpp"
#include "presenter.hpp"

namespace mr {
  class Window : public Presenter, public ResourceBase<Window> {
  private:
    static inline std::once_flag _init_vkfw_flag;
    vkfw::UniqueWindow _window;

    vk::UniqueSurfaceKHR _surface;
    Swapchain _swapchain;

    uint32_t image_index = 0;
    uint32_t prev_image_index = 0;

    // semaphores for waiting swapchain image is ready before light pass
    beman::inplace_vector<vk::UniqueSemaphore, Swapchain::max_images_number> _image_available_semaphore;
    // semaphores for waiting frame is ready before presentin
    beman::inplace_vector<vk::UniqueSemaphore, Swapchain::max_images_number> _render_finished_semaphore;

  public:
    Window(const RenderContext &parent, Extent extent = {800, 600});

    Window(Window &&other) noexcept = default;
    Window &operator=(Window &&other) noexcept = default;
    ~Window() = default;

    vkfw::Window window() { return _window.get(); }

    // Return rendering attachment info with target image
    vk::RenderingAttachmentInfoKHR target_image_info() noexcept final;
    void present() noexcept final;

    void update_state() noexcept final;

  private:
    static vkfw::UniqueWindow _create_window(const Extent &extent) noexcept;
  };

  MR_DECLARE_HANDLE(Window);
} // namespace mr
#endif // __MR_WINDOW_HPP_
