#if !defined(__command_unit_hpp_)
#define __command_unit_hpp_

#include "pch.hpp"

#include "vulkan_state.hpp"

namespace mr {
  class CommandUnit {
    private:
      vk::UniqueCommandPool _cmd_pool;
      vk::CommandBuffer _cmd_buffer;

    public:
      CommandUnit() = default;

      CommandUnit(const VulkanState &state);

      void begin();
      void end();

      std::tuple<vk::CommandBuffer *, uint> submit_info()
      {
        return {&_cmd_buffer, 1};
      }

      vk::CommandBuffer *operator->() { return &_cmd_buffer; }
  };
} // namespace mr

#endif // __command_unit_hpp_
