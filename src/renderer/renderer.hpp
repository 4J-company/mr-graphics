#if !defined(__renderer_hpp_)
  #define __renderer_hpp_

  #include "pch.hpp"

  #include "resources/resources.hpp"
  #include "timer/timer.hpp"
  #include "vulkan_application.hpp"
  #include "window_context/window.hpp"

namespace mr
{
  class Application : private Kernel
  {
  private:
    VulkanState _state;
    VkDebugUtilsMessengerEXT _debug_messenger;

  public:
    Application();
    ~Application();

    // resource creators
    [[nodiscard]] std::unique_ptr<CommandUnit> create_command_unit() const;
    [[nodiscard]] std::unique_ptr<Buffer> create_buffer() const;
    [[nodiscard]] std::unique_ptr<Shader> create_shader() const;
    [[nodiscard]] std::unique_ptr<Pipeline> create_graphics_pipeline() const;
    [[nodiscard]] std::unique_ptr<Pipeline> create_compute_pipeline() const;

    // window creator
    [[nodiscard]] std::unique_ptr<Window> create_window(size_t width, size_t height) const;

    /// REAL SHIT BELOW
    vk::Semaphore _image_available_semaphore;
    vk::Semaphore _render_rinished_semaphore;
    vk::Fence _fence;
  };
} // namespace mr

#endif // __renderer_hpp_
