#include "resources/command_unit/command_unit.hpp"

// destructor
ter::CommandUnit::~CommandUnit() {}

ter::CommandUnit::CommandUnit(VulkanState state)
{
  vk::CommandPoolCreateInfo pool_create_info {
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = 0 /// TODO: correct index
  };

  _cmd_pool = state.device().createCommandPool(pool_create_info).value;

  vk::CommandBufferAllocateInfo cmd_buffer_alloc_info {
      .commandPool = _cmd_pool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  };
  state.device().allocateCommandBuffers(&cmd_buffer_alloc_info, &_cmd_buffer);
}

void ter::CommandUnit::begin()
{
  _cmd_buffer.reset();
  vk::CommandBufferBeginInfo begin_info {}; /// .flags = 0, .pInheritanceInfo = nullptr, };
  _cmd_buffer.begin(begin_info);
}

void ter::CommandUnit::end()
{
  _cmd_buffer.end();
}
