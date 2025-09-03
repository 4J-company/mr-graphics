#ifndef __MR_WINDOW_HPP_
#define __MR_WINDOW_HPP_

#include "pch.hpp"
#include "camera/camera.hpp"
#include "swapchain.hpp"
#include "resources/images/image.hpp"

namespace mr {
inline namespace graphics {
  // forward declaration of Scene class
  class Scene;

  // forward declaration of RenderContext class
  class RenderContext;

  class Window : public ResourceBase<Window> {
  private:
    Extent _extent;
    vkfw::UniqueWindow _window;

    vk::UniqueSurfaceKHR _surface;
    std::optional<Swapchain> _swapchain;

    uint32_t image_index = 0;
    uint32_t prev_image_index = 0;

    // semaphores for waiting swapchain image is ready before light pass
    beman::inplace_vector<vk::UniqueSemaphore, Swapchain::max_images_number> _image_available_semaphore;
    // semaphores for waiting frame is ready before presentin
    beman::inplace_vector<vk::UniqueSemaphore, Swapchain::max_images_number> _render_finished_semaphore;

    const RenderContext *_parent = nullptr;

  public:
    Window(const RenderContext &parent, Extent extent = {800, 600});

    Window(Window &&other) noexcept = default;
    Window &operator=(Window &&other) noexcept = default;
    ~Window() = default;

    Extent extent() const noexcept { return _extent; }

    vkfw::Window window() { return _window.get(); }
    const RenderContext & render_context() const noexcept { return *_parent; }

    // Return rendering attachment info with target image
    vk::RenderingAttachmentInfoKHR get_target_image() noexcept;
    void present() noexcept;

    // Pass this semaphore to render pass wait semaphores witch write in image
    vk::Semaphore image_ready_semaphore() noexcept;
    // Pass this semaphore to render pass signal semaphore witch write in image
    vk::Semaphore render_finished_semaphore() noexcept;
  };

  MR_DECLARE_HANDLE(Window);
} // namespace mr
#endif // __MR_WINDOW_HPP_
