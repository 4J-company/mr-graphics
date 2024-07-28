#if !defined(__window_context_hpp_)
#define __window_context_hpp_

#include "pch.hpp"
#include "resources/resources.hpp"
#include "vulkan_application.hpp"

namespace mr {
  class Window;

  class WindowContext {
    public:
      static inline const uint gbuffers_number = 6;

    private:
      Window *_parent;
      VulkanState _state;
      vk::UniqueSurfaceKHR _surface;
      vk::Format _swapchain_format;
      vk::Extent2D _extent;
      vk::UniqueSwapchainKHR _swapchain;

      std::map<std::string, Shader> _shaders;
      std::array<GraphicsPipeline, 10> _pipelines;
      size_t _pipelines_size = 0;

      std::array<Framebuffer, Framebuffer::max_presentable_images> _framebuffers;
      std::array<Image, gbuffers_number> _gbuffers;
      Image _depthbuffer;

      vk::UniqueRenderPass _render_pass;

      vk::UniqueSemaphore _image_available_semaphore;
      vk::UniqueSemaphore _render_finished_semaphore;
      vk::UniqueFence _image_fence;

public:
      WindowContext() = default;
      WindowContext(Window *parent, const VulkanState &state);
      WindowContext(WindowContext &&other) noexcept = default;
      WindowContext &operator=(WindowContext &&other) noexcept = default;

      ~WindowContext();

      void create_framebuffers(const VulkanState &state);
      void create_depthbuffer(const VulkanState &state);
      void create_render_pass(const VulkanState &state);
      void resize(size_t width, size_t height);
      void render();


      [[nodiscard]] Shader * create_shader(std::string_view filename);
      [[nodiscard]] GraphicsPipeline * create_graphics_pipeline(
        vk::RenderPass render_pass, GraphicsPipeline::Subpass subpass,
        Shader *shader,
        std::span<const vk::DescriptorSetLayout> descriptor_layouts);
  };
} // namespace mr
#endif // __window_context_hpp_
