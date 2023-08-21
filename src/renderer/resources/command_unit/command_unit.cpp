#include "resources/command_unit/command_unit.hpp"

// destructor
ter::CommandUnit::~CommandUnit() {}

ter::CommandUnit::CommandUnit(VulkanApplication &va)
{
  vk::CommandPoolCreateInfo pool_create_info
  {
    .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
    .queueFamilyIndex = 0 /// TODO: correct index
  };

  vk::Result result;
  std::tie(result, _cmd_pool) = va.get_device().createCommandPool(pool_create_info);
  assert(result == vk::Result::eSuccess);

  vk::CommandBufferAllocateInfo cmd_buffer_alloc_info
  {
    .commandPool = _cmd_pool,
    .level = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = 1,
  };
  std::vector<vk::CommandBuffer> cmd_bufs;
  std::tie(result, cmd_bufs) = va.get_device().allocateCommandBuffers(cmd_buffer_alloc_info);
  assert(result == vk::Result::eSuccess);
  _cmd_buffer = cmd_bufs[0];
}

void ter::CommandUnit::begin()
{
  _cmd_buffer.reset();
  vk::CommandBufferBeginInfo begin_info {};/// .flags = 0, .pInheritanceInfo = nullptr, };
  auto result = _cmd_buffer.begin(begin_info);
  assert(result == vk::Result::eSuccess);
}

void ter::CommandUnit::end()
{
  vkCmdEndRenderPass(_cmd_buffer);
  auto result = _cmd_buffer.end();
  assert(result == vk::Result::eSuccess);
}

// record
void ter::CommandUnit::record(/* callable, args[] */) {}
