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
  ASSERT(offset + src.size() <= _size, "offset, src.size()", offset, src.size());

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
// Vector vertex buffer
// ----------------------------------------------------------------------------

mr::VertexVectorBuffer::VertexVectorBuffer(const VulkanState &state, size_t start_byte_size)
  : VectorBuffer(state,
                vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                start_byte_size)
{
}

// ----------------------------------------------------------------------------
// GPU Heap
// ----------------------------------------------------------------------------

static bool is_pow2(uint32_t n) {
  if (n == 1) {
    return true;
  }
  auto log2_of_n = std::log2(double(n));
  return std::abs(log2_of_n - double(int(log2_of_n))) <= 0.0001;
}

mr::GpuHeap::GpuHeap(VkDeviceSize start_byte_size, VkDeviceSize alignment)
  : _size(0)
  , _alignment(alignment)
{
  ASSERT(is_pow2(alignment));
  add_block(start_byte_size, 0);
}

mr::GpuHeap::~GpuHeap() noexcept
{
  for (auto &block : _blocks) {
    vmaClearVirtualBlock(block.virtual_block);
    vmaDestroyVirtualBlock(block.virtual_block);
  }
}

mr::GpuHeap::AllocationBlock & mr::GpuHeap::add_block(VkDeviceSize block_size, VkDeviceSize offset) noexcept
{
  AllocationBlock block(block_size, offset);
  VmaVirtualBlockCreateInfo virtual_block_create_info {
    .size = block_size,
  };
  vmaCreateVirtualBlock(&virtual_block_create_info, &block.virtual_block);
  // TODO(dk6): make it threadsafe
  _blocks.emplace_back(std::move(block));
  _size += block_size;
  return _blocks.back();
}

mr::GpuHeap::AllocInfo mr::GpuHeap::allocate(VkDeviceSize allocation_size) noexcept
{
  ASSERT(allocation_size % _alignment == 0);

  Allocation allocation { .byte_size = allocation_size };
  VkDeviceSize offset;
  VmaVirtualAllocationCreateInfo alloc_info {
    .size = allocation_size,
    .alignment = _alignment,
  };

  bool allocated = false;
  for (auto &&[i, block] : std::views::enumerate(_blocks)) {
    if (vmaVirtualAllocate(block.virtual_block, &alloc_info, &allocation.allocation, &offset) == VK_SUCCESS) {
      offset += block.offset;
      allocation.block_number = i;
      block.allocation_number++;
      allocated = true;
      break;
    }
  }

  if (not allocated) {
    // TODO(dk6): make it threadsafe
    allocation.block_number = _blocks.size();
    const auto &last_block = _blocks.back();
    auto &new_block = add_block((last_block.size + allocation_size) * 2, last_block.offset + last_block.size);
    auto result = vmaVirtualAllocate(new_block.virtual_block, &alloc_info, &allocation.allocation, &offset);
    ASSERT(result == VK_SUCCESS);
    offset += new_block.offset;
    new_block.allocation_number++;
  }

  ASSERT(offset % _alignment == 0);

  // TODO(dk6): make it thread safe critical section
  _allocations.insert({offset, allocation});
  return AllocInfo {offset, not allocated};
}

void mr::GpuHeap::deallocate(VkDeviceSize offset) noexcept
{
  ASSERT(_allocations.contains(offset));
  // TODO(dk6): make it thread safe critical section
  auto &allocation = _allocations[offset];
  vmaVirtualFree(_blocks[allocation.block_number].virtual_block, allocation.allocation);
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

VkDeviceSize mr::HeapBuffer::add_data(std::span<const std::byte> src) noexcept
{
  auto alloc = _heap.allocate(src.size());
  if (alloc.resized) {
    _buffer.resize(_heap.size());
  }
  _buffer.write(src, alloc.offset);
  return alloc.offset;
}

void mr::HeapBuffer::free_data(VkDeviceSize offset) noexcept
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


