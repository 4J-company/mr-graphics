#if !defined(__renderer_hpp_)
  #define __renderer_hpp_

  #include "pch.hpp"

  #include "resources/resources.hpp"
  #include "window_context/window_context.hpp"

namespace ter
{
  class Application : window_system::Application
  {
    friend class WindowContext;

  private:
    vk::Instance _instance;
    vk::Device _device;
    vk::PhysicalDevice _phys_device;
  #if __DEBUG
    vk::DebugUtilsMessengerEXT _dbg_messenger;
  #endif

  public:
    Application();
    ~Application();

    [[nodiscard]] std::unique_ptr<CommandUnit> create_command_unit();
    [[nodiscard]] std::unique_ptr<Buffer> create_buffer();
  };
} // namespace ter

#endif // __renderer_hpp_
