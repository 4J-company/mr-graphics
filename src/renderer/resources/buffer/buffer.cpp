#include "resources/buffer/buffer.hpp"

// constructor
mr::Buffer::Buffer(const VulkanState &state, size_t byte_size,
                   vk::BufferUsageFlags usage_flag,
                   vk::MemoryPropertyFlags memory_properties)
    : _size(byte_size)
    , _usage_flags(usage_flag)
    , _memory_properties(memory_properties)
{
  vk::BufferCreateInfo buffer_create_info {
    .size = _size,
    .usage = _usage_flags,
    .sharingMode = vk::SharingMode::eExclusive,
  };

  _buffer = state.device().createBufferUnique(buffer_create_info).value;

  vk::MemoryRequirements mem_requirements =
    state.device().getBufferMemoryRequirements(_buffer.get());
  vk::MemoryAllocateInfo alloc_info {
    .allocationSize = mem_requirements.size,
    .memoryTypeIndex = find_memory_type(
      state, mem_requirements.memoryTypeBits, _memory_properties),
  };
  _memory = state.device().allocateMemoryUnique(alloc_info).value;
  state.device().bindBufferMemory(_buffer.get(), _memory.get(), 0);
}

// resize
void resize(size_t size) {}

// find memory type
mr::uint mr::Buffer::find_memory_type(const VulkanState &state, uint filter,
                                      vk::MemoryPropertyFlags properties) noexcept
{
  vk::PhysicalDeviceMemoryProperties mem_properties =
    state.phys_device().getMemoryProperties();

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
    if ((filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags &
                                properties) == properties) {
      return i;
    }
  }

  assert(false); // cant find format
  return 0;
}
