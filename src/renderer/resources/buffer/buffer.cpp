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
  std::tie(_buffer, _allocation) = create_buffer(state, usage_flags, memory_properties, byte_size);
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

mr::DeviceBuffer & mr::DeviceBuffer::write(std::span<const std::byte> src, VkDeviceSize offset)
{
  ASSERT(_state != nullptr);
  ASSERT(src.data());
  ASSERT(offset + src.size() <= _size, "data offset + data size overflow buffer", offset, src.size());

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

// vk::BufferUsageFlagBits::eTransferDst |
mr::DrawIndirectBuffer::DrawIndirectBuffer(const VulkanState &state,
                                           vk::BufferUsageFlags additional_usage,
                                           uint32_t command_size,
                                           uint32_t start_draws_count)
  : VectorBuffer(state,
                 vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | additional_usage,
                 command_size * start_draws_count)
  , _draws_count(start_draws_count)
  , _command_size(command_size)
{
}

void mr::DrawIndirectBuffer::resize_draws_count(uint32_t draws_count) noexcept
{
  resize(draws_count * _command_size);
  _draws_count = draws_count;
}

// ----------------------------------------------------------------------------
// Vector buffer
// ----------------------------------------------------------------------------

mr::VectorBuffer::VectorBuffer(const VulkanState &state,
                             vk::BufferUsageFlags usage_flags,
                             VkDeviceSize start_byte_size)
  : DeviceBuffer(state, start_byte_size, usage_flags | vk::BufferUsageFlagBits::eTransferSrc)
  , _usage_flags(usage_flags)
  , _current_size(0)
{
}

VkDeviceSize mr::VectorBuffer::push_data_back(std::span<const std::byte> src) noexcept
{
  VkDeviceSize offset = _current_size;
  _current_size += src.size();
  if (_current_size > _size) {
    recreate_buffer(_current_size * 2);
  }
  write(src, offset);
  return offset;
}

void mr::VectorBuffer::resize(VkDeviceSize new_size) noexcept
{
  if (new_size > _size) {
    recreate_buffer(new_size);
  }
  _current_size = new_size;
}

void mr::VectorBuffer::recreate_buffer(VkDeviceSize new_size) noexcept
{
  auto &&[buffer, allocation] = create_buffer(*_state, _usage_flags | vk::BufferUsageFlagBits::eTransferSrc, {}, new_size);
  vk::BufferCopy buffer_copy {
    .srcOffset = 0,
    .dstOffset = 0,
    .size = _size,
  };
  // TODO(dk6): pass RenderContext to buffer and get from it
  CommandUnit command_unit(*_state);
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
// Vector vertex buffer
// ----------------------------------------------------------------------------

mr::VertexVectorBuffer::VertexVectorBuffer(const VulkanState &state, size_t start_byte_size)
  : VectorBuffer(state,
                vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                start_byte_size)
{
}

// ----------------------------------------------------------------------------
// Allocation block of GPU heap
// ----------------------------------------------------------------------------

mr::DeviceHeapAllocator::AllocationBlock::AllocationBlock(VkDeviceSize size, VkDeviceSize offset, uint32_t block_number) noexcept
  : _size(size), _offset(offset), _block_number(block_number)
{
  VmaVirtualBlockCreateInfo virtual_block_create_info {
    .size = _size,
  };
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

std::optional<std::pair<VkDeviceSize, mr::DeviceHeapAllocator::Allocation>>
mr::DeviceHeapAllocator::AllocationBlock::allocate(VkDeviceSize allocation_size, uint32_t alignment) noexcept
{
  Allocation allocation {
    .byte_size = allocation_size,
    .block_number = _block_number,
  };

  VkDeviceSize offset;
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

static bool is_pow2(uint32_t n) {
  return std::bitset<32>(n).count() == 1;
}

mr::DeviceHeapAllocator::DeviceHeapAllocator(VkDeviceSize start_byte_size, VkDeviceSize alignment)
  : _size(0)
  , _alignment(alignment)
{
  ASSERT(is_pow2(alignment));
  add_block(start_byte_size);
}

mr::DeviceHeapAllocator & mr::DeviceHeapAllocator::operator=(DeviceHeapAllocator &&other) noexcept
{
  std::swap(_alignment, other._alignment);
  _size = other._size.load();
  _allocations = std::move(_allocations);
  _blocks = std::move(_blocks);
  return *this;
}

mr::DeviceHeapAllocator::AllocationBlock & mr::DeviceHeapAllocator::add_block(VkDeviceSize allocation_size) noexcept
{
  VkDeviceSize block_size = allocation_size;
  VkDeviceSize offset = 0;
  // TBB vector give us method's thread safety (we use it for correct ittertation over _blocks in 'allocate')
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

mr::DeviceHeapAllocator::AllocInfo mr::DeviceHeapAllocator::allocate(VkDeviceSize allocation_size) noexcept
{
  ASSERT(allocation_size % _alignment == 0);

  std::pair<VkDeviceSize, Allocation> allocation;

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

  std::lock_guard lock(_allocations_mutex);
  _allocations.insert(allocation);
  return AllocInfo {allocation.first, not allocated};
}

void mr::DeviceHeapAllocator::deallocate(VkDeviceSize offset) noexcept
{
  std::lock_guard lock(_allocations_mutex);
  auto allocation_it = _allocations.find(offset);
  ASSERT(allocation_it != _allocations.end(),
         "Tried to deallocate non-allocated memory block (probably a double-free)", offset);
  auto &allocation = allocation_it->second;
  _blocks[allocation.block_number].deallocate(allocation);
  _allocations.erase(offset);
}

// ----------------------------------------------------------------------------
// Heap buffer
// ----------------------------------------------------------------------------

mr::HeapBuffer::HeapBuffer(const VulkanState &state,
                           vk::BufferUsageFlags usage_flags,
                           VkDeviceSize start_byte_size,
                           VkDeviceSize alignment)
  : _buffer(state, usage_flags, start_byte_size)
  , _heap(start_byte_size, alignment)
{
}

VkDeviceSize mr::HeapBuffer::allocate(VkDeviceSize size) noexcept
{
  auto alloc = _heap.allocate(size);
  if (alloc.resized) {
    _buffer.resize(_heap.size());
  }
  return alloc.offset;
}

VkDeviceSize mr::HeapBuffer::allocate_and_write(std::span<const std::byte> src) noexcept
{
  auto offset = allocate(src.size());
  write(src, offset);
  return offset;
}

void mr::HeapBuffer::free(VkDeviceSize offset) noexcept
{
  _heap.deallocate(offset);
}

// ----------------------------------------------------------------------------
// Heap vertex buffer
// ----------------------------------------------------------------------------

mr::VertexHeapBuffer::VertexHeapBuffer(const VulkanState &state, VkDeviceSize start_byte_size, VkDeviceSize alignment)
  : HeapBuffer(state,
               vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
               start_byte_size,
               alignment)
{
}

// ----------------------------------------------------------------------------
// Heap index buffer
// ----------------------------------------------------------------------------

mr::IndexHeapBuffer::IndexHeapBuffer(const VulkanState &state, VkDeviceSize start_byte_size, VkDeviceSize alignment)
  : HeapBuffer(state,
               vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
               start_byte_size,
               alignment)
{
}


