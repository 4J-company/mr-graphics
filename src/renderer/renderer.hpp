#if !defined(renderer)
#define renderer

#include "pch.hpp"

namespace ter
{  class command_unit
  {
    friend class application;

  private:
    vk::CommandPool _cmd_pool;
    std::array<vk::CommandBuffer, 3> _cmd_buffer;

  public:
    command_unit() = default;
  };

  class application
  {
  private:
    vk::Instance _instance;
    vk::Device _device;
#if __DEBUG
    vk::DebugUtilsMessengerEXT _dbg_messenger;
#endif

  public:
    application();
    ~application();

    [[no_discard]] std::unique_ptr<command_unit> create_command_unit();
  };
}

#endif
