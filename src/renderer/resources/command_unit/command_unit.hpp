#ifndef __MR_COMMAND_UNIT_HPP_
#define __MR_COMMAND_UNIT_HPP_

#include "pch.hpp"

#include "vulkan_state.hpp"

namespace mr {
inline namespace graphics {
  class HostBuffer;
  class DeviceBuffer;

  class CommandUnit {
  private:
    constexpr static size_t max_semaphores_number = 10;

    vk::UniqueCommandPool _cmd_pool;
    vk::UniqueCommandBuffer _cmd_buffer;

    std::pair<
      InplaceVector<vk::Semaphore, max_semaphores_number>,
      InplaceVector<vk::PipelineStageFlags, max_semaphores_number>
    > _wait_sems;
    InplaceVector<vk::Semaphore, max_semaphores_number> _signal_sems;
    std::vector<HostBuffer> _staging_buffers;
    std::vector<DeviceBuffer> _resized_from_buffers;
    // TODO(dk6): maybe also add some fences?

  public:
    CommandUnit() = default;

    CommandUnit(const VulkanState &state);

    void begin();
    void end();
    vk::SubmitInfo submit_info() const noexcept;

    void clear_semaphores() noexcept;

    void add_wait_semaphore(vk::Semaphore sem, vk::PipelineStageFlags stage_flags) noexcept;
    void add_signal_semaphore(vk::Semaphore sem) noexcept;

    // Buffers that were used as temporary staging buffers during write commands.
    std::vector<HostBuffer> & staging_buffers() noexcept { return _staging_buffers; }
    // Buffers that were resized and are now sources to write commands.
    std::vector<DeviceBuffer> & resized_from_buffers() noexcept { return _resized_from_buffers; }

    vk::CommandBuffer command_buffer() { return _cmd_buffer.get(); }
    vk::CommandBuffer * operator->() { return &_cmd_buffer.get(); }

    vk::UniqueFence submit(const VulkanState &state) noexcept {
      auto fence = state.device().createFenceUnique({}).value;
      state.queue().submit(submit_info(), fence.get());
      return std::move(fence);
    }
  };

  struct FenceGuard {
    vk::Device device {};
    vk::Fence fence {};
    FenceGuard(vk::Device d, vk::Fence f = {}) : fence(f), device(d) {}
    ~FenceGuard() noexcept { if (fence) { device.waitForFences({fence}, vk::True, UINT64_MAX); } }
  };

  struct UniqueFenceGuard {
    vk::Device device {};
    vk::UniqueFence fence {};
    UniqueFenceGuard(vk::Device d, vk::UniqueFence f) : fence(std::move(f)), device(d) {}
    ~UniqueFenceGuard() noexcept { if (fence) { device.waitForFences({fence.get()}, vk::True, UINT64_MAX); } }
  };
}
} // namespace mr

#endif // __MR_COMMAND_UNIT_HPP_
