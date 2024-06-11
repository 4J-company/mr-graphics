#if !defined(__buffer_hpp_)
#define __buffer_hpp_

#include "pch.hpp"


#include "resources/command_unit/command_unit.hpp"
#include "vulkan_application.hpp"

namespace mr {
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

#endif // __buffer_hpp_
