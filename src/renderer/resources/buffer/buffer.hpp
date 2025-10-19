#ifndef __MR_BUFFER_HPP_
#define __MR_BUFFER_HPP_

#include "pch.hpp"

#include <vk_mem_alloc.h>

#include "vulkan_state.hpp"
#include <vulkan/vulkan_core.h>

namespace mr {
inline namespace graphics {
  class Buffer {
  public:
    Buffer() = default;
    Buffer(const VulkanState &state, size_t byte_size,
           vk::BufferUsageFlags usage_flags,
           vk::MemoryPropertyFlags memory_properties);
    Buffer(Buffer &&other) noexcept {
      std::swap(_state, other._state);
      std::swap(_size, other._size);
      std::swap(_buffer, other._buffer);
      std::swap(_allocation, other._allocation);
    }
    Buffer & operator=(Buffer &&other) noexcept {
      std::swap(_state, other._state);
      std::swap(_size, other._size);
      std::swap(_buffer, other._buffer);
      std::swap(_allocation, other._allocation);
      return *this;
    }
    virtual ~Buffer() noexcept;

    void resize(size_t byte_size);

    static uint find_memory_type(const VulkanState &state, uint filter,
                                 vk::MemoryPropertyFlags properties) noexcept;

    vk::Buffer buffer() const noexcept { return _buffer; }

    size_t byte_size() const noexcept { return _size; }

  protected:
    const VulkanState *_state = nullptr;

    size_t _size = 0;

    vk::Buffer _buffer {};

    VmaAllocation _allocation {};
  };

  class HostBuffer : public Buffer {
  private:
    class MappedData {
    private:
      HostBuffer *_buf = nullptr;
      void *_data = nullptr;

    public:

      MappedData(HostBuffer &buf) : _buf(&buf) {}
      ~MappedData() { if (mapped()) { unmap(); } }

      MappedData & operator=(MappedData &&other) noexcept;
      MappedData(MappedData &&other) noexcept;

      MappedData & operator=(const MappedData &other) = delete;
      MappedData(const MappedData &other) = delete;

      void * map() noexcept;
      void unmap() noexcept;
      bool mapped() const noexcept { return _data != nullptr; }
      void * get() noexcept { return _data; }
    };

    MappedData _mapped_data;

  public:
    ~HostBuffer();

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

    HostBuffer(HostBuffer &&) = default;
    HostBuffer &operator=(HostBuffer &&) = default;

    // This method just map device memory and collect it to span
    std::span<const std::byte> read() noexcept;

    HostBuffer &write(std::span<const std::byte> src);

    // This method copy to CPU memory and unmap device memory
    std::vector<std::byte> copy() noexcept;

    template <typename T, size_t Extent>
    HostBuffer &write(std::span<T, Extent> src) { return write(std::as_bytes(src)); }
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

    template <size_t Extent>
    DeviceBuffer &write(std::span<const std::byte, Extent> src) { return write(std::span<const std::byte>(src.data(), src.size())); }

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
    UniformBuffer(const VulkanState &state, size_t size);

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

    StorageBuffer(const VulkanState &state, size_t byte_size);

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
      static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4,
                    "Unsupported index element size for IndexBuffer");

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

  class DrawIndirectBuffer : public DeviceBuffer {
  private:
    uint32_t _max_draws_count;

    // TODO(dk6): Maybe it can be unneccessary in future
    bool _fill_from_cpu = false;
    // TODO(dk6): make it threadsafe
    bool _updated = true;
    std::vector<vk::DrawIndexedIndirectCommand> _draws;

  public:
    DrawIndirectBuffer() = default;

    DrawIndirectBuffer(const VulkanState &state, uint32_t max_draws_count, bool fill_from_cpu = true);

    DrawIndirectBuffer(DrawIndirectBuffer &&) noexcept = default;
    DrawIndirectBuffer & operator=(DrawIndirectBuffer &&) noexcept = default;

    void add_command(const vk::DrawIndexedIndirectCommand &command) noexcept;
    void clear() noexcept;
    void update() noexcept;
  };

}
} // namespace mr

#endif // __MR_BUFFER_HPP_
