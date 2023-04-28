#if !defined(cmd_unit)
#define cmd_unit

#include "pch.hpp"

namespace ter
{
  class command_unit
  {
    friend class application;

  private:
    vk::CommandPool _cmd_pool;
    std::array<vk::CommandBuffer, 3> _cmd_buffer;

  public:
    command_unit() = default;
  };
}

#endif
