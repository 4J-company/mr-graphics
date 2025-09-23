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
    friend class FileWriter; // TODO(dk6): tmp solution for copying from buffer to RAM

    private:
      class MappedData {
      private:
        HostBuffer *_buf = nullptr;
        void *_data = nullptr;

      public:
        MappedData(HostBuffer &buf);
        ~MappedData();

        MappedData & operator=(MappedData &&other) noexcept;
        MappedData(MappedData &&other) noexcept;

        void * map(size_t offset = 0) noexcept;
        void * map(size_t offset, size_t size) noexcept;
        void unmap() noexcept;
        bool mapped() const noexcept;
        void * get() noexcept;
      };

      MappedData _mapped_data;

    public:
    HostBuffer(
      const VulkanState &state, std::size_t size,
      vk::BufferUsageFlags usage_flags,
      vk::MemoryPropertyFlags memory_properties = vk::MemoryPropertyFlags(0))
        : Buffer(state, size, usage_flags,
                 memory_properties |
                   vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent)
        , _mapped_data(*this)
    {
    }

    ~HostBuffer();

    HostBuffer(HostBuffer &&) = default;
    HostBuffer &operator=(HostBuffer &&) = default;

    HostBuffer &write(std::span<const std::byte> src);

    template <typename T, size_t Extent>
    HostBuffer &write(std::span<T, Extent> src) { return write(std::as_bytes(src)); }

    // This method just map device memory and collect it to span
    std::span<std::byte> read() noexcept;

    // This method copy to CPU memory and unmap device memory
    std::vector<std::byte> copy() noexcept;
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

    DeviceBuffer &write(std::span<const std::byte> src);

    template <typename T, size_t Extent>
    DeviceBuffer &write(std::span<T, Extent> src) { return write(std::as_bytes(src)); }
  };

  class ConditionalBuffer : public DeviceBuffer {
  public:
    ConditionalBuffer() = default;
    ConditionalBuffer(ConditionalBuffer &&) noexcept = default;
    ConditionalBuffer & operator=(ConditionalBuffer &&) noexcept = default;

    ConditionalBuffer(const VulkanState &state, size_t byte_size)
      : DeviceBuffer(state, byte_size,
                     vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
                       vk::BufferUsageFlagBits::eConditionalRenderingEXT)
    {
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
      ASSERT(src.data());
      write(src);
    }
  };

  class StorageBuffer : public DeviceBuffer {
    public:
      using DeviceBuffer::DeviceBuffer;

      StorageBuffer(const VulkanState &state, size_t byte_size)
          : DeviceBuffer(state, byte_size,
                         vk::BufferUsageFlagBits::eStorageBuffer |
                           vk::BufferUsageFlagBits::eTransferDst)
      {
      }

      template <typename T, size_t Extent>
      StorageBuffer(const VulkanState &state, std::span<T, Extent> src)
          : StorageBuffer(state, src.size() * sizeof(T))
      {
        assert(src.data());
        write(src);
      }
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
