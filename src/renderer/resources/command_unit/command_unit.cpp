#include "resources/command_unit/command_unit.hpp"

// destructor
ter::CommandUnit::~CommandUnit() {}

ter::CommandUnit::CommandUnit(VulkanState va)
{
  vk::CommandPoolCreateInfo pool_create_info {
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = 0 /// TODO: correct index
  };

  vk::Result result;
  std::tie(result, _cmd_pool) = va.device().createCommandPool(pool_create_info);
  assert(result == vk::Result::eSuccess);

  vk::CommandBufferAllocateInfo cmd_buffer_alloc_info {
      .commandPool = _cmd_pool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  };
  result = va.device().allocateCommandBuffers(&cmd_buffer_alloc_info, &_cmd_buffer);
  assert(result == vk::Result::eSuccess);
}

void ter::CommandUnit::begin()
{
  _cmd_buffer.reset();
  vk::CommandBufferBeginInfo begin_info {}; /// .flags = 0, .pInheritanceInfo = nullptr, };
  auto result = _cmd_buffer.begin(begin_info);
  assert(result == vk::Result::eSuccess);
}

void ter::CommandUnit::end()
{
  auto result = _cmd_buffer.end();
  assert(result == vk::Result::eSuccess);
}
