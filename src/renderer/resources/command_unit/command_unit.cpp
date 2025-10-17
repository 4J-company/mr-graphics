#include "resources/command_unit/command_unit.hpp"

mr::CommandUnit::CommandUnit(const VulkanState &state)
{
  vk::CommandPoolCreateInfo pool_create_info {
    .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
    .queueFamilyIndex = 0 /// TODO: correct index
  };

  // _cmd_pool = state.device().createCommandPool(pool_create_info).value;
  _cmd_pool = state.device().createCommandPoolUnique(pool_create_info).value;

  vk::CommandBufferAllocateInfo cmd_buffer_alloc_info {
    .commandPool = _cmd_pool.get(),
    .level = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = 1,
  };
  auto cmd_buffers = state.device().allocateCommandBuffersUnique(cmd_buffer_alloc_info).value;
  _cmd_buffer = std::move(cmd_buffers[0]);
}

void mr::CommandUnit::begin()
{
  clear_semaphores();
  _cmd_buffer->reset();
  vk::CommandBufferBeginInfo
    begin_info {}; /// .flags = 0, .pInheritanceInfo = nullptr, };
  _cmd_buffer->begin(begin_info);
}

void mr::CommandUnit::end()
{
  _cmd_buffer->end();
}

vk::SubmitInfo mr::CommandUnit::submit_info() const noexcept {
  auto &[wait_sems, wait_stage_flags] = _wait_sems;
  vk::SubmitInfo submit_info {
    .waitSemaphoreCount = static_cast<uint32_t>(wait_sems.size()),
    .pWaitSemaphores = wait_sems.data(),
    .pWaitDstStageMask = wait_stage_flags.data(),
    .commandBufferCount = 1,
    .pCommandBuffers = &_cmd_buffer.get(),
    .signalSemaphoreCount = static_cast<uint32_t>(_signal_sems.size()),
    .pSignalSemaphores = _signal_sems.data(),
  };
  return submit_info;
}

void mr::CommandUnit::clear_semaphores() noexcept
{
  auto &[wait_sems, wait_stage_flags] = _wait_sems;
  wait_sems.clear();
  wait_stage_flags.clear();
  _signal_sems.clear();
}

void mr::CommandUnit::add_wait_semaphore(vk::Semaphore sem, vk::PipelineStageFlags stage_flags) noexcept
{
  ASSERT(_wait_sems.first.size() + 1 < max_semaphores_number, "Not enough space for wait semaphores");
  auto &[wait_sems, wait_stage_flags] = _wait_sems;
  wait_sems.emplace_back(sem);
  wait_stage_flags.emplace_back(stage_flags);
}

void mr::CommandUnit::add_signal_semaphore(vk::Semaphore sem) noexcept
{
  ASSERT(_signal_sems.size() + 1 < max_semaphores_number, "Not enough space for signal semaphores");
  _signal_sems.emplace_back(sem);
}

