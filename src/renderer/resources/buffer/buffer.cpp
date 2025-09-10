#include "resources/buffer/buffer.hpp"

// constructor
mr::Buffer::Buffer(const VulkanState &state, size_t byte_size,
                   vk::BufferUsageFlags usage_flag,
                   vk::MemoryPropertyFlags memory_properties)
  : _state(&state)
  , _size(byte_size)
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
uint32_t mr::Buffer::find_memory_type(
    const VulkanState &state,
    uint32_t filter,
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

  ASSERT(false, "Cannot find memory format");
  return 0;
}

mr::DeviceBuffer & mr::DeviceBuffer::write(std::span<const std::byte> src)
{
  ASSERT(_state != nullptr);
  ASSERT(src.data());
  ASSERT(src.size() <= _size);

  auto buf = HostBuffer(*_state, _size, vk::BufferUsageFlagBits::eTransferSrc);
  buf.write(src);

  vk::BufferCopy buffer_copy {.size = src.size()};

  static CommandUnit command_unit(*_state);
  command_unit.begin();
  command_unit->copyBuffer(buf.buffer(), _buffer.get(), {buffer_copy});
  command_unit.end();

  auto [bufs, bufs_number] = command_unit.submit_info();
  vk::SubmitInfo submit_info {
    .commandBufferCount = bufs_number,
    .pCommandBuffers = bufs,
  };

  auto fence = _state->device().createFenceUnique({}).value;
  _state->queue().submit(submit_info, fence.get());
  _state->device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

  return *this;
}

mr::HostBuffer & mr::HostBuffer::write(std::span<const std::byte> src)
{
  ASSERT(_state != nullptr);
  ASSERT(src.data());
  ASSERT(src.size() <= _size);

  auto ptr = _state->device().mapMemory(_memory.get(), 0, src.size()).value;
  memcpy(ptr, src.data(), src.size());
  _state->device().unmapMemory(_memory.get());
  return *this;
}

std::span<std::byte> mr::HostBuffer::read() noexcept
{
  if (_mapped_data == nullptr) {
    _state->device().mapMemory(_memory.get(), 0, _size, {}, &_mapped_data);
  }
  return std::span(reinterpret_cast<std::byte *>(_mapped_data), _size);
}

std::vector<std::byte> mr::HostBuffer::copy() noexcept
{
  std::vector<std::byte> data(_size);
  if (_mapped_data != nullptr) {
    memcpy(data.data(), _mapped_data, _size);
  } else {
    _state->device().mapMemory(_memory.get(), 0, _size, {}, &_mapped_data);
    memcpy(data.data(), _mapped_data, _size);
    _state->device().unmapMemory(_memory.get());
  }
  return data;
}

mr::HostBuffer::~HostBuffer()
{
  // TODO(dk6): Now, while _mapped_data is simple pointer, it can be bad after move, double memory unmap
  if (_mapped_data != nullptr) {
    _state->device().unmapMemory(_memory.get());
    _mapped_data = nullptr;
  }
}
