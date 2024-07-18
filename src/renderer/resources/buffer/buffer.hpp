#if !defined(__buffer_hpp_)
#define __buffer_hpp_

#include "pch.hpp"


#include "resources/command_unit/command_unit.hpp"
#include "vulkan_state.hpp"

namespace mr {
  class Buffer {
    protected:
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
      Buffer(const VulkanState &state, size_t byte_size,
             vk::BufferUsageFlags usage_flag,
             vk::MemoryPropertyFlags memory_properties);
      Buffer(Buffer &&) = default;
      Buffer &operator=(Buffer &&) = default;
      virtual ~Buffer() = 0;

      void resize(size_t byte_size);

      static uint find_memory_type(const VulkanState &state, uint filter,
                                   vk::MemoryPropertyFlags properties) noexcept;

      vk::Buffer buffer() const noexcept { return _buffer.get(); }

      size_t byte_size() const noexcept { return _size; }
  };

  inline Buffer::~Buffer() {}

  class HostBuffer : public Buffer {
    public:
      HostBuffer() = default;

      HostBuffer(
        const VulkanState &state, std::size_t size,
        vk::BufferUsageFlags usage_flags,
        vk::MemoryPropertyFlags memory_properties = vk::MemoryPropertyFlags(0))
          : Buffer(state, size, usage_flags,
                   memory_properties |
                     vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent)
      {
      }

      HostBuffer(HostBuffer &&) = default;
      HostBuffer &operator=(HostBuffer &&) = default;

      template <typename T, size_t Extent>
      HostBuffer &write(const VulkanState &state, std::span<T, Extent> src)
      {
        // std::span<const T, Extent> cannot be constructed from non-const

        size_t byte_size = src.size() * sizeof(T);
        assert(byte_size <= _size);

        auto ptr = state.device().mapMemory(_memory.get(), 0, byte_size).value;
        memcpy(ptr, src.data(), byte_size);
        state.device().unmapMemory(_memory.get());
        return *this;
      }
  };

  class DeviceBuffer : public Buffer {
    public:
      DeviceBuffer() = default;
      DeviceBuffer(DeviceBuffer &&) = default;
      DeviceBuffer &operator=(DeviceBuffer &&) = default;

      DeviceBuffer(
        const VulkanState &state, std::size_t size,
        vk::BufferUsageFlags usage_flags,
        vk::MemoryPropertyFlags memory_properties = vk::MemoryPropertyFlags(0))
          : Buffer(state, size, usage_flags,
                   memory_properties | vk::MemoryPropertyFlagBits::eDeviceLocal)
      {
      }

      template <typename T, size_t Extent>
      DeviceBuffer &write(const VulkanState &state, std::span<T, Extent> src)
      {
        // std::span<const T, Extent> cannot be constructed from non-const

        size_t byte_size = src.size() * sizeof(T);
        assert(byte_size <= _size);

        auto buf =
          HostBuffer(state, _size, vk::BufferUsageFlagBits::eTransferSrc);
        buf.write(state, src);

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
      UniformBuffer(const VulkanState &state, size_t size)
          : HostBuffer(state, size, vk::BufferUsageFlagBits::eUniformBuffer)
      {
      }

      template <typename T, size_t Extent>
      UniformBuffer(const VulkanState &state, std::span<T, Extent> src)
          : UniformBuffer(state, src.size() * sizeof(T))
      {
        assert(src.data());
        write(state, src);
      }
  };

  class StorageBuffer : public DeviceBuffer {
    public:
      using DeviceBuffer::DeviceBuffer;
  };

  class VertexBuffer : public DeviceBuffer {
    public:
      VertexBuffer() = default;

      VertexBuffer(const VulkanState &state, size_t byte_size)
          : DeviceBuffer(state, byte_size,
                         vk::BufferUsageFlagBits::eVertexBuffer |
                           vk::BufferUsageFlagBits::eTransferDst)
      {
      }

      VertexBuffer(VertexBuffer &&) noexcept = default;
      VertexBuffer &operator=(VertexBuffer &&) noexcept = default;

      template <typename T, size_t Extent>
      VertexBuffer(const VulkanState &state, std::span<T, Extent> src)
          : VertexBuffer(state, src.size() * sizeof(T))
      {
        assert(src.data());
        write(state, src);
      }
  };

  class IndexBuffer : public DeviceBuffer {
    public:
      IndexBuffer() = default;

      IndexBuffer(const VulkanState &state, size_t byte_size)
          : DeviceBuffer(state, byte_size,
                         vk::BufferUsageFlagBits::eIndexBuffer |
                           vk::BufferUsageFlagBits::eTransferDst)
      {
      }

      IndexBuffer(IndexBuffer &&) noexcept = default;
      IndexBuffer &operator=(IndexBuffer &&) noexcept = default;

      template <typename T, size_t Extent>
      IndexBuffer(const VulkanState &state, std::span<T, Extent> src)
          : IndexBuffer(state, src.size() * sizeof(T))
      {
        assert(src.data());
        write(state, src);
      }
  };
} // namespace mr

#endif // __buffer_hpp_
