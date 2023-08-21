#if !defined(__renderer_hpp_)
#define __renderer_hpp_

#include "pch.hpp"

#include "vulkan_application.hpp"
#include "resources/resources.hpp"
#include "timer/timer.hpp"
#include "window_context/window_context.hpp"

namespace ter
{
  class Application : public window_system::Application, public VulkanApplication
  {
  public:
    friend class WindowContext;

    CommandUnit _cmd_unit;
    std::vector<WindowContext *> _window_contexts;

    Application();
    ~Application();

    [[nodiscard]] std::unique_ptr<CommandUnit> create_command_unit();
    [[nodiscard]] std::unique_ptr<Buffer> create_buffer();

    vk::Instance & get_instance() { return _instance; }

    /// REAL SHIT BELOW
    Shader _shader;
    GraphicsPipeline _pipeline;

    vk::Semaphore _image_available_semaphore;
    vk::Semaphore _render_rinished_semaphore;
    vk::Fence _fence;

    void create_render_resourses();
    void render();
  };

  class triangle_redner
  {
  public:
  };
} // namespace ter

#endif // __renderer_hpp_
