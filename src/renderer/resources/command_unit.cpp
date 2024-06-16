module;
#include "pch.hpp"
export module CommandUnit;

import VulkanApplication;

export namespace mr {
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
  state.device().allocateCommandBuffers(&cmd_buffer_alloc_info, &_cmd_buffer);
}

void mr::CommandUnit::begin()
{
  _cmd_buffer.reset();
  vk::CommandBufferBeginInfo
    begin_info {}; /// .flags = 0, .pInheritanceInfo = nullptr, };
  _cmd_buffer.begin(begin_info);
}

void mr::CommandUnit::end()
{
  _cmd_buffer.end();
}
