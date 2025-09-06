#ifndef __MR_WINDOW_HPP_
#define __MR_WINDOW_HPP_

#include "pch.hpp"
#include "swapchain.hpp"
#include "input_state.hpp"
#include "presenter.hpp"

namespace mr {
inline namespace graphics {
  class Window {
  private:
    Extent _extent;
    vkfw::UniqueWindow _window;
    std::optional<mr::RenderContext> _context;
    mr::FPSCamera *_cam;

    vk::UniqueSurfaceKHR _surface;
    Swapchain _swapchain;

    uint32_t image_index = 0;
    uint32_t prev_image_index = 0;

    InputState _input_state;

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

    void update_state() noexcept;
    const InputState & input_state() const noexcept { return _input_state; }

  private:
    static vkfw::UniqueWindow _create_window(const Extent &extent) noexcept;
  };

  MR_DECLARE_HANDLE(Window);
}
} // namespace mr
#endif // __MR_WINDOW_HPP_
