#if !defined(__buffer_hpp_)
#define __buffer_hpp_

#include "pch.hpp"


#include "resources/command_unit/command_unit.hpp"
#include "vulkan_application.hpp"

namespace mr {
  class Buffer {
    protected:
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
      Buffer(Buffer&&) = default;
      Buffer & operator=(Buffer&&) = default;
      virtual ~Buffer() = 0;

      void resize(size_t byte_size);

      static uint find_memory_type(const VulkanState &state, uint filter,
                                   vk::MemoryPropertyFlags properties);

      const vk::Buffer buffer() { return _buffer.get(); }

      const size_t byte_size() { return _size; }
  };

  inline Buffer::~Buffer() {}

  class HostBuffer : public Buffer {
    public:

    using Buffer::Buffer;

    template <typename T>
      Buffer &write(const VulkanState &state, T *data, std::size_t size)
      {
        size_t byte_size = size * sizeof(T);

        assert(byte_size <= _size);

        auto ptr = state.device().mapMemory(_memory.get(), 0, byte_size).value;
        memcpy(ptr, data, _size);
        state.device().unmapMemory(_memory.get());
        return *this;
      }
  };

  class DeviceBuffer : public Buffer {
    public:
    using Buffer::Buffer;

    template <typename T>
      Buffer &write(const VulkanState &state, T *data, std::size_t size)
      {
        size_t byte_size = size * sizeof(T);

        assert(byte_size <= _size);

        auto buf = HostBuffer(state,
            _size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
        buf.write(state, data, size);

        vk::BufferCopy buffer_copy {.size = byte_size};

        static CommandUnit command_unit(state);
        command_unit.begin();
        command_unit->copyBuffer(buf.buffer(), _buffer.get(), {buffer_copy});
        command_unit.end();

        auto [bufs, bufs_number] = command_unit.submit_info();
        vk::SubmitInfo submit_info {
          .commandBufferCount = bufs_number,
            .pCommandBuffers = bufs,
        };
        auto fence = state.device().createFence({}).value;
        state.queue().submit(submit_info, fence);
        state.device().waitForFences({fence}, VK_TRUE, UINT64_MAX);

        return *this;
      }
  };

  class UniformBuffer : public HostBuffer {
    public:

    using HostBuffer::HostBuffer;
    using HostBuffer::Buffer;

  UniformBuffer(const VulkanState &state, size_t size)
   : HostBuffer(
      state,
      size,
      vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    ) {}

  template <typename T>
    UniformBuffer(const VulkanState &state, T *data, size_t size)
    : UniformBuffer(state, size)
    {
      assert(data);
      write(state, data, size);
    }
  };

  class StorageBuffer : public DeviceBuffer {
    public:

    using DeviceBuffer::DeviceBuffer;
    using DeviceBuffer::Buffer;
  };

  class VertexBuffer  : public DeviceBuffer {
    public:

    using DeviceBuffer::DeviceBuffer;
    using DeviceBuffer::Buffer;

    VertexBuffer(const VulkanState &state, size_t size)
      : DeviceBuffer(state, size,
          vk::BufferUsageFlagBits::eVertexBuffer |
          vk::BufferUsageFlagBits::eTransferDst,
          vk::MemoryPropertyFlagBits::eDeviceLocal)
    {}

    template <typename type = float>
      VertexBuffer(const VulkanState &state, type *data, size_t size) : VertexBuffer(state, size)
    {
      assert(data);
      write(state, data, _size);
    }
  };

  class IndexBuffer : public DeviceBuffer {
    public:

    using DeviceBuffer::DeviceBuffer;
    using DeviceBuffer::Buffer;

    IndexBuffer(const VulkanState &state, size_t size)
     : DeviceBuffer(state,
          size,
          vk::BufferUsageFlagBits::eIndexBuffer |
          vk::BufferUsageFlagBits::eTransferDst,
          vk::MemoryPropertyFlagBits::eDeviceLocal)
    {}

    template <typename T>
      IndexBuffer(const VulkanState &state, T *data, size_t size) : IndexBuffer(state, size)
    {
      assert(data);
      write(state, data, size);
    }
  };
} // namespace mr

#endif // __buffer_hpp_

