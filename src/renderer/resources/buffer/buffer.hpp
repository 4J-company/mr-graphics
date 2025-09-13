#ifndef __MR_BUFFER_HPP_
#define __MR_BUFFER_HPP_

#include "pch.hpp"


#include "resources/command_unit/command_unit.hpp"
#include "vulkan_state.hpp"

namespace mr {
inline namespace graphics {
  class Buffer {
    public:
      Buffer() = default;
      Buffer(const VulkanState &state, size_t byte_size,
             vk::BufferUsageFlags usage_flag,
             vk::MemoryPropertyFlags memory_properties);
      Buffer(Buffer &&) = default;
      Buffer &operator=(Buffer &&) = default;
      virtual ~Buffer() = default;

      void resize(size_t byte_size);

      static uint find_memory_type(const VulkanState &state, uint filter,
                                   vk::MemoryPropertyFlags properties) noexcept;

      vk::Buffer buffer() const noexcept { return _buffer.get(); }

      size_t byte_size() const noexcept { return _size; }

    protected:
      const VulkanState *_state{nullptr};

      size_t _size;

      vk::UniqueBuffer _buffer;
      vk::UniqueDeviceMemory _memory;

      vk::BufferUsageFlags _usage_flags;
      vk::MemoryPropertyFlags _memory_properties;
      // VMA data
      // VmaAllocation _allocation;
      // VmaMemoryUsage _memory_usage;
  };

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
      HostBuffer &write(std::span<T, Extent> src)
      {
        // std::span<const T, Extent> cannot be constructed from non-const

        size_t byte_size = src.size() * sizeof(T);
        ASSERT(byte_size <= _size);

        ASSERT(_state != nullptr);
        auto ptr = _state->device().mapMemory(_memory.get(), 0, byte_size).value;
        memcpy(ptr, src.data(), byte_size);
        _state->device().unmapMemory(_memory.get());
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
      DeviceBuffer &write(std::span<T, Extent> src)
      {
        // std::span<const T, Extent> cannot be constructed from non-const

        size_t byte_size = src.size() * sizeof(T);
        ASSERT(byte_size <= _size);

        ASSERT(_state != nullptr);
        auto buf =
          HostBuffer(*_state, _size, vk::BufferUsageFlagBits::eTransferSrc);
        buf.write(src);

        vk::BufferCopy buffer_copy {.size = byte_size};

        static std::mutex record_mtx;
        static std::mutex submit_mtx;
        static CommandUnit command_unit(*_state);

        {
          std::lock_guard lock(record_mtx);
          command_unit.begin();
          command_unit->copyBuffer(buf.buffer(), _buffer.get(), {buffer_copy});
          command_unit.end();
        }

        auto [bufs, bufs_number] = command_unit.submit_info();
        vk::SubmitInfo submit_info {
          .commandBufferCount = bufs_number,
          .pCommandBuffers = bufs,
        };
        auto fence = _state->device().createFenceUnique({}).value;
        {
          std::lock_guard lock(submit_mtx);
          _state->queue().submit(submit_info, fence.get());
        }
        _state->device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

        return *this;
      }
  };

  class UniformBuffer : public HostBuffer {
    public:
      UniformBuffer() = default;

      UniformBuffer(const VulkanState &state, size_t size)
          : HostBuffer(state, size, vk::BufferUsageFlagBits::eUniformBuffer)
      {
      }

      template <typename T, size_t Extent>
      UniformBuffer(const VulkanState &state, std::span<T, Extent> src)
          : UniformBuffer(state, src.size() * sizeof(T))
      {
        ASSERT(src.data());
        write(src);
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
        ASSERT(src.data());
        write(src);
      }
  };

  class IndexBuffer : public DeviceBuffer {
    private:
      vk::IndexType _index_type;
      std::size_t _element_count;

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
        ASSERT(src.data());
        write(src);

        _element_count = src.size();

        // determine index type
        if constexpr (sizeof(T) == 4) {
          _index_type = vk::IndexType::eUint32;
        }
        else if constexpr (sizeof(T) == 2) {
          _index_type = vk::IndexType::eUint16;
        }
        else if constexpr (sizeof(T) == 1) {
          _index_type = vk::IndexType::eUint8KHR;
        }
      }

      std::size_t   element_count() const noexcept { return _element_count; }
      vk::IndexType index_type() const noexcept { return _index_type; }
  };
}
} // namespace mr

#endif // __MR_BUFFER_HPP_
