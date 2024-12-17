#if !defined(__window_context_hpp_)
#define __window_context_hpp_

#include "pch.hpp"
#include "resources/resources.hpp"
#include "vulkan_state.hpp"
#include "utils/misc.hpp"

namespace mr {
  class Window;

  class RenderContext {
    public:
      static inline const uint gbuffers_number = 6;

      RenderContext() = default;
      RenderContext(RenderContext &&other) noexcept = default;
      RenderContext &operator=(RenderContext &&other) noexcept = default;

      RenderContext(VulkanGlobalState *state, Window *parent);

      ~RenderContext();

      void resize(Extent extent);
      void render();

    private:
      void _create_swapchain();
      void _create_framebuffers();
      void _create_depthbuffer();
      void _create_render_pass();

      Window *_parent;
      VulkanState _state;
      Extent _extent;

      vk::UniqueSurfaceKHR _surface;
      vk::Format _swapchain_format{vk::Format::eB8G8R8A8Unorm};
      vk::UniqueSwapchainKHR _swapchain;

      std::array<Framebuffer, Framebuffer::max_presentable_images> _framebuffers;
      std::array<Image, gbuffers_number> _gbuffers;
      Image _depthbuffer;

      vk::UniqueRenderPass _render_pass;

      vk::UniqueSemaphore _image_available_semaphore;
      vk::UniqueSemaphore _render_finished_semaphore;
      vk::UniqueFence _image_fence;
  };
} // namespace mr
#endif // __window_context_hpp_
