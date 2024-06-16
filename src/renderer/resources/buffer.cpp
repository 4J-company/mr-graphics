module;
#include "pch.hpp"
export module Buffer;

import CommandUnit;
import VulkanApplication;

export namespace mr {
  class Buffer {
    private:
      void *_host_data;
      size_t _size;

      vk::UniqueBuffer _buffer;
      vk::UniqueDeviceMemory _memory;

      vk::BufferUsageFlags _usage_flags;
      vk::MemoryPropertyFlags _memory_properties;
      // VMA data
      // VmaAllocation _allocation;
      // VmaMemoryUsage _memory_usage;

    public:
      Buffer() = default;
      Buffer(const VulkanState &state, size_t size,
             vk::BufferUsageFlags usage_flag,
             vk::MemoryPropertyFlags memory_properties);

      // resize
      void resize(size_t size);

      template <typename type>
      Buffer &write(const VulkanState &state, type *data)
      {
        auto ptr = state.device().mapMemory(_memory.get(), 0, _size).value;
        memcpy(ptr, data, _size);
        state.device().unmapMemory(_memory.get());
        return *this;
      }

      template <typename type>
      Buffer &write_gpu(const VulkanState &state, type *data)
      {
        auto buf = Buffer(state,
                          _size,
                          vk::BufferUsageFlagBits::eTransferSrc,
                          vk::MemoryPropertyFlagBits::eHostVisible |
                            vk::MemoryPropertyFlagBits::eHostCoherent);
        buf.write(state, data);

        vk::BufferCopy buffer_copy {.size = _size};

        static CommandUnit command_unit(state);
        command_unit.begin();
        command_unit->copyBuffer(buf.buffer(), _buffer.get(), {buffer_copy});
        command_unit.end();

        auto [bufs, size] = command_unit.submit_info();
        vk::SubmitInfo submit_info {
          .commandBufferCount = size,
          .pCommandBuffers = bufs,
        };
        auto fence = state.device().createFence({}).value;
        state.queue().submit(submit_info, fence);
        state.device().waitForFences({fence}, VK_TRUE, UINT64_MAX);

        return *this;
      }

      // find memory tppe
      static uint find_memory_type(const VulkanState &state, uint filter,
                                   vk::MemoryPropertyFlags properties);

      const vk::Buffer buffer() { return _buffer.get(); }

      const size_t size() { return _size; }
  };

  class UniformBuffer : public Buffer { using Buffer::Buffer; };
  class StorageBuffer : public Buffer { using Buffer::Buffer; };
  class VertexBuffer  : public Buffer { using Buffer::Buffer; };
  class IndexBuffer   : public Buffer { using Buffer::Buffer; };
} // namespace mr

// constructor
mr::Buffer::Buffer(const VulkanState &state, size_t size,
                   vk::BufferUsageFlags usage_flag,
                   vk::MemoryPropertyFlags memory_properties)
    : _size(size)
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
                                      vk::MemoryPropertyFlags properties)
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
