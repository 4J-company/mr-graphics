#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "resources/buffer/buffer.hpp"

#include "window/render_context.hpp"

// constructor
mr::Buffer::Buffer(const VulkanState &state, size_t byte_size,
                   vk::BufferUsageFlags usage_flags,
                   vk::MemoryPropertyFlags memory_properties)
  : _state(&state)
  , _size(byte_size)
{
  auto res = create_buffer(state, usage_flags, memory_properties, byte_size);
  _buffer = std::move(res.first);
  _allocation = std::move(res.second);
}

mr::Buffer::~Buffer() noexcept {
  if (_buffer != VK_NULL_HANDLE) {
    ASSERT(_state != nullptr);
    vmaDestroyBuffer(_state->allocator(), _buffer, _allocation);
    _buffer = VK_NULL_HANDLE;
  }
}

std::pair<vk::Buffer, VmaAllocation> mr::Buffer::create_buffer(const VulkanState &state,
                                                               vk::BufferUsageFlags usage_flags,
                                                               vk::MemoryPropertyFlags memory_properties,
                                                               size_t byte_size)
{
  vk::BufferCreateInfo buffer_create_info {
    .size = byte_size,
    .usage = usage_flags,
    .sharingMode = vk::SharingMode::eExclusive,
  };

  VmaAllocationCreateInfo allocation_create_info { };
  allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

  if (memory_properties & vk::MemoryPropertyFlagBits::eHostVisible) {
    allocation_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  }

  vk::Buffer buffer;
  VmaAllocation allocation;

  auto result = vmaCreateBuffer(
    state.allocator(),
    (VkBufferCreateInfo *)&buffer_create_info,
    &allocation_create_info,
    (VkBuffer *)&buffer,
    &allocation,
    nullptr
  );

  if (result != VK_SUCCESS) {
#ifndef NDEBUG
    auto budgets = state.memory_budgets();
    for (uint32_t heapIndex = 0; heapIndex < VK_MAX_MEMORY_HEAPS; heapIndex++) {
      if (budgets[heapIndex].statistics.allocationCount == 0) continue;
      MR_DEBUG("My heap currently has {} allocations taking {} B,",
          budgets[heapIndex].statistics.allocationCount,
          budgets[heapIndex].statistics.allocationBytes);
      MR_DEBUG("allocated out of {} Vulkan device memory blocks taking {} B,",
          budgets[heapIndex].statistics.blockCount,
          budgets[heapIndex].statistics.blockBytes);
      MR_DEBUG("Vulkan reports total usage {} B with budget {} B ({}%).",
          budgets[heapIndex].usage,
          budgets[heapIndex].budget,
          budgets[heapIndex].usage / (float)budgets[heapIndex].budget);
    }
#endif
    ASSERT(false, "Failed to create vk::Buffer", result);
  }

  return {buffer, allocation};
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

std::span<const std::byte> mr::HostBuffer::read() noexcept
{
  if (not _mapped_data.mapped()) {
    _mapped_data.map();
  }
  return std::span(reinterpret_cast<const std::byte *>(_mapped_data.get()), _size);
}

std::vector<std::byte> mr::HostBuffer::copy() noexcept
{
  std::vector<std::byte> data(_size);
  if (_mapped_data.mapped()) {
    memcpy(data.data(), _mapped_data.get(), _size);
  } else {
    memcpy(data.data(), _mapped_data.map(), _size);
    _mapped_data.unmap();
  }
  return data;
}

mr::HostBuffer & mr::HostBuffer::write(std::span<const std::byte> src)
{
  ASSERT(_state != nullptr);
  ASSERT(src.data());
  ASSERT(src.size() <= _size);

  if (_mapped_data.mapped()) {
    memcpy(_mapped_data.get(), src.data(), src.size());
  } else {
    memcpy(_mapped_data.map(), src.data(), src.size());
    _mapped_data.unmap();
  }

  return *this;
}

mr::HostBuffer::~HostBuffer() {}

// ----------------------------------------------------------------------------
// Data mapper
// ----------------------------------------------------------------------------

mr::HostBuffer::MappedData & mr::HostBuffer::MappedData::operator=(MappedData &&other) noexcept
{
  std::swap(_buf, other._buf);
  std::swap(_data, other._data);
  return *this;
}

mr::HostBuffer::MappedData::MappedData(MappedData &&other) noexcept
{
  std::swap(_buf, other._buf);
  std::swap(_data, other._data);
}

void * mr::HostBuffer::MappedData::map() noexcept
{
  ASSERT(_buf != nullptr);
  ASSERT(_buf->_state != nullptr);
  ASSERT(_data == nullptr);

  auto result = vmaMapMemory(_buf->_state->allocator(), _buf->_allocation, &_data);

  ASSERT(result == VK_SUCCESS, "Failed to map memory for the vk::Buffer", result);
  ASSERT(_data != nullptr);
  return _data;
}

void mr::HostBuffer::MappedData::unmap() noexcept
{
  ASSERT(_buf != nullptr);
  ASSERT(_buf->_state != nullptr);
  ASSERT(_data != nullptr);

  vmaUnmapMemory(_buf->_state->allocator(), _buf->_allocation);
  _data = nullptr;
}

mr::DeviceBuffer & mr::DeviceBuffer::write(std::span<const std::byte> src, uint32_t offset)
{
  ASSERT(_state != nullptr);
  ASSERT(src.data());
  ASSERT(offset + src.size() <= _size);

  auto buf = HostBuffer(*_state, _size, vk::BufferUsageFlagBits::eTransferSrc);
  buf.write(src);

  vk::BufferCopy buffer_copy {
    .dstOffset = offset,
    .size = src.size()
  };

  CommandUnit command_unit(*_state);
  command_unit.begin();
  command_unit->copyBuffer(buf.buffer(), _buffer, {buffer_copy});
  command_unit.end();

  vk::SubmitInfo submit_info  = command_unit.submit_info();

  auto fence = _state->device().createFenceUnique({}).value;
  _state->queue().submit(submit_info, fence.get());
  _state->device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

  return *this;
}

// ----------------------------------------------------------------------------
// Uniform buffer
// ----------------------------------------------------------------------------

mr::UniformBuffer::UniformBuffer(const VulkanState &state, size_t size)
  : HostBuffer(state, size, vk::BufferUsageFlagBits::eUniformBuffer)
{
}

// ----------------------------------------------------------------------------
// Storage buffer
// ----------------------------------------------------------------------------

mr::StorageBuffer::StorageBuffer(const VulkanState &state, size_t byte_size)
  : DeviceBuffer(state, byte_size,
                 vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
{
}

// ----------------------------------------------------------------------------
// Draw inderect buffer
// ----------------------------------------------------------------------------

mr::DrawIndirectBuffer::DrawIndirectBuffer(const VulkanState &state, uint32_t max_draws_count, bool fill_from_cpu)
  : DeviceBuffer(state, sizeof(vk::DrawIndexedIndirectCommand) * max_draws_count,
                 vk::BufferUsageFlagBits::eStorageBuffer |
                 vk::BufferUsageFlagBits::eTransferDst |
                 vk::BufferUsageFlagBits::eIndirectBuffer)
  , _max_draws_count(max_draws_count)
  , _fill_from_cpu(fill_from_cpu)
{
  if (_fill_from_cpu) {
    _draws.reserve(max_draws_count);
  }
}

void mr::DrawIndirectBuffer::add_command(const vk::DrawIndexedIndirectCommand &command) noexcept
{
  ASSERT(_fill_from_cpu, "add_commmand method is only for indirect buffer which can be filled from cpu");
  ASSERT(_draws.size() <= _max_draws_count,
         "not enough space for indirect command storing, increase size of indirect buffer");
  _draws.emplace_back(command);
  _updated = false;
}

void mr::DrawIndirectBuffer::clear() noexcept
{
  ASSERT(_fill_from_cpu, "clear method is only for indirect buffer which can be filled from cpu");
  _draws.clear();
  _updated = false;
}

void mr::DrawIndirectBuffer::update() noexcept
{
  ASSERT(_fill_from_cpu, "update method is only for indirect buffer which can be filled from cpu");
  if (not _updated) {
    write(std::span(_draws));
    _updated = true;
  }
}

// ----------------------------------------------------------------------------
// Heap buffer
// ----------------------------------------------------------------------------

mr::HeapBuffer::HeapBuffer(const VulkanState &state,
                           vk::BufferUsageFlags usage_flags,
                           size_t start_byte_size,
                           uint32_t alignment)
  :
  DeviceBuffer(state, start_byte_size, usage_flags | vk::BufferUsageFlagBits::eTransferSrc)
  , _aligment(alignment)
  , _usage_flags(usage_flags | vk::BufferUsageFlagBits::eTransferSrc)
{
  VmaVirtualBlockCreateInfo virtual_block_create_info {
    .size = start_byte_size,
  };
  vmaCreateVirtualBlock(&virtual_block_create_info, &_virtual_block);
}

uint32_t mr::HeapBuffer::add_data(std::span<const std::byte> src) noexcept
{
  VmaVirtualAllocationCreateInfo alloc_info {
    .size = src.size_bytes(),
    .alignment = _aligment,
  };

  Allocation allocation;
  VkDeviceSize offset;
  for (int retry = 0; retry < 2; retry++) {
    if (vmaVirtualAllocate(_virtual_block, &alloc_info, &allocation.allocation, &offset) == VK_SUCCESS) {
      break;
    }
    ASSERT(retry == 0, "Can not allocate enough space for vertex buffer");
    size_t new_size = (_size + src.size_bytes()) * 2;
    recreate_buffer(new_size);
  }

  ASSERT(src.size() % _aligment == 0);
  // ASSERT(allocation.offset % _aligment == 0);

  allocation.byte_size = src.size_bytes();
  write(src, offset);

  // TODO(dk6): make it thread safe critical section
  _allocations.insert({offset, allocation});
  return static_cast<uint32_t>(offset);
}

void mr::HeapBuffer::free_data(uint32_t offset) noexcept
{
  auto &allocation = _allocations[offset];
  vmaVirtualFree(_virtual_block, allocation.allocation);
  _allocations.erase(offset);
}

void mr::HeapBuffer::recreate_buffer(size_t new_size) noexcept
{
  MR_INFO("Recreating buffer");
  VmaVirtualBlock new_block;
  VmaVirtualBlockCreateInfo virtual_block_create_info {
    .size = new_size,
  };
  vmaCreateVirtualBlock(&virtual_block_create_info, &new_block);
  auto &&[new_buffer, new_allocation] = create_buffer(*_state, _usage_flags, vk::MemoryPropertyFlags(0), new_size);

  // TODO(dk6): We can do it without dealocation for each write. For this:
  // Recreate buffer, but don't tell it to allocations, just copy data
  // Have array of virtual allocations + real offset:
  //   (size, 0)
  //   (size * 2, size)
  // Append new allocation virtual block to end: (size * 4, size + size * 2 = size * 3)
  // We have the log itteration for find block with enough space, but cheaper recreation - i don't know what is better

  uint32_t previous_offset = 0;
  for (auto &[offset, virtual_allocation] : _allocations) {
    ASSERT(previous_offset <= offset);
    previous_offset = offset;

    VmaVirtualAllocationCreateInfo allocation_info {
      .size = virtual_allocation.byte_size,
      .alignment = _aligment,
      .flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT
    };

    VkDeviceSize new_offset;
    VmaVirtualAllocation new_allocation;
    auto result = vmaVirtualAllocate(new_block, &allocation_info,
                                     &new_allocation,
                                     &new_offset);
    ASSERT(result == VK_SUCCESS);
    ASSERT(offset == new_offset);

    virtual_allocation.allocation = new_allocation;
  }

  // copy data
  vk::BufferCopy copy_info {
    .srcOffset = 0,
    .dstOffset = 0,
    .size = _size,
  };
  // TODO(dk6): pass RenderContext to buffer and get from it
  static CommandUnit command_unit(*_state);
  command_unit.begin();
  command_unit->copyBuffer(_buffer, new_buffer, {copy_info});
  command_unit.end();

  vk::SubmitInfo submit_info  = command_unit.submit_info();

  auto fence = _state->device().createFenceUnique({}).value;
  _state->queue().submit(submit_info, fence.get());
  _state->device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

  // finilize buffer recreating
  vmaClearVirtualBlock(_virtual_block);
  vmaDestroyVirtualBlock(_virtual_block);
  vmaDestroyBuffer(_state->allocator(), _buffer, _allocation);

  _size = new_size;
  _buffer = std::move(new_buffer);
  _allocation = std::move(new_allocation);
  _virtual_block = std::move(new_block);
}

mr::HostBuffer mr::HeapBuffer::read() const noexcept
{
  auto buf = HostBuffer(*_state, _size, vk::BufferUsageFlagBits::eTransferDst);
  vk::BufferCopy copy_info {
    .srcOffset = 0,
    .dstOffset = 0,
    .size = _size
  };
  // TODO(dk6): pass RenderContext to buffer and get from it
  static CommandUnit command_unit(*_state);
  command_unit.begin();
  command_unit->copyBuffer(_buffer, buf.buffer(), {copy_info});
  command_unit.end();

  vk::SubmitInfo submit_info  = command_unit.submit_info();

  auto fence = _state->device().createFenceUnique({}).value;
  _state->queue().submit(submit_info, fence.get());
  _state->device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

  return buf;
}

// ----------------------------------------------------------------------------
// Heap vertex buffer
// ----------------------------------------------------------------------------

mr::VertexHeapBuffer::VertexHeapBuffer(const VulkanState &state, size_t start_byte_size, uint32_t alignment)
  : HeapBuffer(state,
               vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
               start_byte_size,
               alignment)
{
}

// ----------------------------------------------------------------------------
// Heap index buffer
// ----------------------------------------------------------------------------

mr::IndexHeapBuffer::IndexHeapBuffer(const VulkanState &state, size_t start_byte_size, uint32_t alignment)
  : HeapBuffer(state,
               vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
               start_byte_size,
               alignment)
{
}

// ----------------------------------------------------------------------------
// Stack buffer
// ----------------------------------------------------------------------------

mr::StackBuffer::StackBuffer(const VulkanState &state,
                             vk::BufferUsageFlags usage_flags,
                             size_t start_byte_size)
  : DeviceBuffer(state, start_byte_size, usage_flags | vk::BufferUsageFlagBits::eTransferSrc)
  , _usage_flags(usage_flags)
  , _current_size(0)
{
}

uint32_t mr::StackBuffer::add_data(std::span<const std::byte> src) noexcept
{
  uint32_t offset = _current_size;
  _current_size += src.size();
  if (_current_size > _size) {
    recreate_buffer(_current_size * 2);
  }
  write(src, offset);
  return offset;
}

void mr::StackBuffer::recreate_buffer(size_t new_size) noexcept
{
  auto &&[buffer, allocation] = create_buffer(*_state, _usage_flags, {}, new_size);
  vk::BufferCopy buffer_copy {
    .srcOffset = 0,
    .dstOffset = 0,
    .size = _size, // TODO(dk6): copy only used data, not all previously allocated
  };
  // TODO(dk6): pass RenderContext to buffer and get from it
  static CommandUnit command_unit(*_state);
  command_unit.begin();
  command_unit->copyBuffer(_buffer, buffer, {buffer_copy});
  command_unit.end();

  vk::SubmitInfo submit_info  = command_unit.submit_info();

  auto fence = _state->device().createFenceUnique({}).value;
  _state->queue().submit(submit_info, fence.get());
  _state->device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

  vmaDestroyBuffer(_state->allocator(), _buffer, _allocation);

  _size = new_size;
  _buffer = std::move(buffer);
  _allocation = std::move(allocation);
}

// ----------------------------------------------------------------------------
// Stack vertex buffer
// ----------------------------------------------------------------------------

mr::VertexStackBuffer::VertexStackBuffer(const VulkanState &state, size_t start_byte_size)
  : StackBuffer(state,
                vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                start_byte_size)
{
}
