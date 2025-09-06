#ifndef __MR_COMMAND_UNIT_HPP_
#define __MR_COMMAND_UNIT_HPP_

#include "pch.hpp"

#include "vulkan_state.hpp"

namespace mr {
inline namespace graphics {
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
}
} // namespace mr

#endif // __MR_COMMAND_UNIT_HPP_
