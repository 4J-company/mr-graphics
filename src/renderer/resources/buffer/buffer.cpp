#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "resources/buffer/buffer.hpp"

#include "window/render_context.hpp"

void mr::bufcopy(mr::CommandUnit &command_unit, mr::BufferRegion src, mr::BufferRegion dst)
{
  ASSERT(src.state == dst.state, "Buffers were created by different VulkanState objects", src, dst);
  ASSERT(dst.size >= src.size, "This copy would cause an overflow", src, dst);
  vk::BufferCopy buffer_copy {
    .srcOffset = src.offset,
    .dstOffset = dst.offset,
    .size = src.size
  };
  command_unit->copyBuffer(src.buffer, dst.buffer, {buffer_copy});
}

// constructor
mr::Buffer::Buffer(const VulkanState &state, size_t byte_size,
                   vk::BufferUsageFlags usage_flags,
                   vk::MemoryPropertyFlags memory_properties)
  : _state(&state)
  , _usage_flags(usage_flags)
  , _size(byte_size)
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

  auto result = vmaCreateBuffer(
    state.allocator(),
    (VkBufferCreateInfo *)&buffer_create_info,
    &allocation_create_info,
    (VkBuffer *)&_buffer,
    &_allocation,
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
    PANIC("Failed to create vk::Buffer", result);
  }
}

mr::Buffer::~Buffer() noexcept {
  if (_buffer != VK_NULL_HANDLE) {
    ASSERT(_state != nullptr);
    vmaDestroyBuffer(_state->allocator(), _buffer, _allocation);
    _buffer = VK_NULL_HANDLE;
  }
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

// ----------------------------------------------------------------------------
// Device buffer
// ----------------------------------------------------------------------------

mr::DeviceBuffer & mr::DeviceBuffer::resize(CommandUnit &command_unit, std::size_t new_size) noexcept
{
  auto &it = command_unit.resized_from_buffers().emplace_back(*_state, new_size, _usage_flags); // create resized temporary
  bufcopy(command_unit, {*this}, {it});                                                         // copy current data to resized temporary
  std::swap(*this, it);                                                                         // reclaim memory of the temporary

  return *this;
}

mr::DeviceBuffer & mr::DeviceBuffer::write(CommandUnit &command_unit,
    std::span<const std::byte> src, vk::DeviceSize offset)
{
  ASSERT(_state != nullptr);
  ASSERT(src.data());
  ASSERT(offset + src.size() <= _size, "This write would cause buffer overflow", offset, src.size());

  auto& it = command_unit.staging_buffers().emplace_back(*_state, src.size(), vk::BufferUsageFlagBits::eTransferSrc);
  it.write(src);

  bufcopy(command_unit, {command_unit.staging_buffers().back()}, {*this, offset});
  return *this;
}

// ----------------------------------------------------------------------------
// Uniform buffer
// ----------------------------------------------------------------------------

mr::UniformBuffer::UniformBuffer(const VulkanState &state, size_t size, vk::BufferUsageFlags usage_flags)
  : HostBuffer(state, size, usage_flags | vk::BufferUsageFlagBits::eUniformBuffer)
{
}

// ----------------------------------------------------------------------------
// Storage buffer
// ----------------------------------------------------------------------------

mr::StorageBuffer::StorageBuffer(
    const VulkanState &state,
    size_t byte_size,
    vk::BufferUsageFlags usage_flags)
  : DeviceBuffer(state,
                 byte_size,
                 usage_flags
                   | vk::BufferUsageFlagBits::eStorageBuffer
                   | vk::BufferUsageFlagBits::eTransferDst
                 )
{
}

// ----------------------------------------------------------------------------
// Vector buffer
// ----------------------------------------------------------------------------

mr::VectorBuffer::VectorBuffer(const VulkanState &state,
                               vk::BufferUsageFlags usage_flags,
                               vk::DeviceSize start_byte_size)
  : DeviceBuffer(state, start_byte_size, usage_flags | vk::BufferUsageFlagBits::eTransferSrc)
  , _current_size(0)
{
}

vk::DeviceSize mr::VectorBuffer::append_range(CommandUnit &command_unit, std::span<const std::byte> src) noexcept
{
  vk::DeviceSize offset = _current_size;
  _current_size += src.size();
  if (_current_size > _size) {
    DeviceBuffer::resize(command_unit, _current_size * resize_coefficient);
  }
  write(command_unit, src, offset);
  return offset;
}

void mr::VectorBuffer::resize(vk::DeviceSize new_size) noexcept
{
  if (new_size > capacity()) {
    CommandUnit command_unit {*_state};
    command_unit.begin();
    DeviceBuffer::resize(command_unit, new_size);
    command_unit.end();

    UniqueFenceGuard(_state->device(), command_unit.submit(*_state));
  }

  _current_size = new_size;
}

// ----------------------------------------------------------------------------
// Vector vertex buffer
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Allocation block of GPU heap
// ----------------------------------------------------------------------------

mr::DeviceHeapAllocator::AllocationBlock::AllocationBlock(
    vk::DeviceSize size, vk::DeviceSize offset, uint32_t block_number) noexcept
  : _size(size), _offset(offset), _block_number(block_number)
{
  VmaVirtualBlockCreateInfo virtual_block_create_info { .size = _size, };
  vmaCreateVirtualBlock(&virtual_block_create_info, &_virtual_block);
}

mr::DeviceHeapAllocator::AllocationBlock::~AllocationBlock() noexcept
{
  if (_virtual_block != nullptr) {
    vmaClearVirtualBlock(_virtual_block);
    vmaDestroyVirtualBlock(_virtual_block);
  }
}

mr::DeviceHeapAllocator::AllocationBlock & mr::DeviceHeapAllocator::AllocationBlock::operator=(AllocationBlock &&other) noexcept
{
  std::swap(_virtual_block, other._virtual_block);
  std::swap(_size, other._size);
  std::swap(_offset, other._offset);
  _allocation_number = other._allocation_number.load();
  std::swap(_block_number, other._block_number);
  return *this;
}

std::optional<std::pair<vk::DeviceSize, mr::DeviceHeapAllocator::Allocation>>
mr::DeviceHeapAllocator::AllocationBlock::allocate(vk::DeviceSize allocation_size, uint32_t alignment) noexcept
{
  Allocation allocation {
    .byte_size = allocation_size,
    .block_number = _block_number,
  };

  vk::DeviceSize offset;
  VmaVirtualAllocationCreateInfo alloc_info {
    .size = allocation_size,
    .alignment = alignment,
  };

  // vmaVirtualAllocate is not threadsafe (deepseek says)
  std::unique_lock lock(_mutex);
  if (vmaVirtualAllocate(_virtual_block, &alloc_info, &allocation.allocation, &offset) != VK_SUCCESS) {
    return std::nullopt;
  }
  lock.unlock();

  offset += _offset;
  allocation.block_number = _block_number;
  _allocation_number++;
  return std::make_pair(offset, allocation);
}

void mr::DeviceHeapAllocator::AllocationBlock::deallocate(Allocation &allocation) noexcept
{
  std::lock_guard lock(_mutex);
  vmaVirtualFree(_virtual_block, allocation.allocation);
  allocation.allocation = nullptr;
}

// ----------------------------------------------------------------------------
// GPU Heap
// ----------------------------------------------------------------------------

mr::DeviceHeapAllocator::DeviceHeapAllocator(vk::DeviceSize start_byte_size, vk::DeviceSize alignment)
  : _size(0)
  , _alignment(alignment)
{
  ASSERT(std::bitset<64>(alignment).count() == 1);
  add_block(start_byte_size);
}

mr::DeviceHeapAllocator & mr::DeviceHeapAllocator::operator=(DeviceHeapAllocator &&other) noexcept
{
  _alignment = other._alignment;
  _size = other._size.load();
  _allocations = std::move(_allocations);
  _blocks = std::move(_blocks);
  return *this;
}

mr::DeviceHeapAllocator::AllocationBlock & mr::DeviceHeapAllocator::add_block(vk::DeviceSize allocation_size) noexcept
{
  vk::DeviceSize block_size = allocation_size;
  vk::DeviceSize offset = 0;
  // TBB vector give us method's thread safety (we use it for correct iteration over _blocks in 'allocate')
  // But in this block we use a lot of different vector's methods, and we want to sequential execution of it.
  std::lock_guard lock(_add_block_mutex);
  if (not _blocks.empty()) {
    auto &last_block = _blocks.back();
    block_size = (last_block._size + allocation_size) * 2;
    offset = last_block._offset + last_block._size;
  }
  _blocks.emplace_back(block_size, offset, _blocks.size());
  _size += block_size;
  return _blocks.back();
}

mr::DeviceHeapAllocator::AllocationInfo mr::DeviceHeapAllocator::allocate(vk::DeviceSize allocation_size) noexcept
{
  ASSERT(allocation_size % _alignment == 0);

  std::pair<vk::DeviceSize, Allocation> allocation;

  bool allocated = false;
  for (auto &block : _blocks) {
    auto &&res = block.allocate(allocation_size, _alignment);
    if (res.has_value()) {
      allocation = std::move(res.value());
      allocated = true;
      break;
    }
  }

  if (not allocated) {
    auto &&res = add_block(allocation_size).allocate(allocation_size, _alignment);
    ASSERT(res.has_value());
    allocation = std::move(res.value());
  }

  ASSERT(allocation.first % _alignment == 0);

  {
    std::shared_lock lock(_allocations_mutex);
    _allocations.insert(allocation);
  }

  return AllocationInfo {allocation.first, not allocated};
}

void mr::DeviceHeapAllocator::deallocate(vk::DeviceSize offset) noexcept
{
  std::unique_lock lock(_allocations_mutex);

  auto allocation_it = _allocations.find(offset);
  ASSERT(allocation_it != _allocations.end(),
         "Tried to deallocate non-allocated memory block (probably a double-free)", offset);
  auto &allocation = allocation_it->second;

  _blocks[allocation.block_number].deallocate(allocation);
  _allocations.unsafe_extract(allocation_it);
}

// ----------------------------------------------------------------------------
// Heap buffer
// ----------------------------------------------------------------------------

mr::HeapBuffer::HeapBuffer(const VulkanState &state,
                           vk::BufferUsageFlags usage_flags,
                           vk::DeviceSize start_byte_size,
                           vk::DeviceSize alignment)
  : _buffer(state, usage_flags, start_byte_size)
  , _heap(start_byte_size, alignment)
{
}

vk::DeviceSize mr::HeapBuffer::allocate(vk::DeviceSize size) noexcept
{
  auto alloc = _heap.allocate(size);
  if (alloc.resized) {
    _buffer.resize(_heap.size());
  }
  return alloc.offset;
}

vk::DeviceSize mr::HeapBuffer::allocate_and_write(CommandUnit &command_unit, std::span<const std::byte> src) noexcept
{
  auto offset = allocate(src.size());
  write(command_unit, src, offset);
  return offset;
}

void mr::HeapBuffer::free(vk::DeviceSize offset) noexcept
{
  _heap.deallocate(offset);
}

