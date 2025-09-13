#ifndef __MR_COMMAND_UNIT_HPP_
#define __MR_COMMAND_UNIT_HPP_

#include "pch.hpp"

#include "vulkan_state.hpp"

namespace mr {
inline namespace graphics {
  class CommandUnit {
    private:
      constexpr static size_t max_semaphores_number = 10;

      vk::UniqueCommandPool _cmd_pool;
      vk::CommandBuffer _cmd_buffer;

      std::pair<
        beman::inplace_vector<vk::Semaphore, max_semaphores_number>,
        beman::inplace_vector<vk::PipelineStageFlags, max_semaphores_number>
      > _wait_sems;
      beman::inplace_vector<vk::Semaphore, max_semaphores_number> _signal_sems;
      // TODO(dk6): maybe also add some fences?

    public:
      CommandUnit() = default;

      CommandUnit(const VulkanState &state);

      void begin();
      vk::SubmitInfo end();

      void clear_semaphores() noexcept;

      void add_wait_semaphore(vk::Semaphore sem, vk::PipelineStageFlags stage_flags) noexcept;
      void add_signal_semaphore(vk::Semaphore sem) noexcept;

      vk::CommandBuffer * operator->() { return &_cmd_buffer; }
  };
}
} // namespace mr

#endif // __MR_COMMAND_UNIT_HPP_
