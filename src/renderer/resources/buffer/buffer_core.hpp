#ifndef __MR_BUFFER_CORE_HPP_
#define __MR_BUFFER_CORE_HPP_

#include <memory>
#include "pch.hpp"

#include <vk_mem_alloc.h>

#include "vulkan_state.hpp"
#include <vulkan/vulkan_core.h>

namespace mr {
inline namespace graphics {
  class CommandUnit;

  class Buffer {
  protected:
    const VulkanState *_state = nullptr;

    vk::DeviceSize _size = 0;
    vk::Buffer _buffer {};
    vk::BufferUsageFlags _usage_flags {};
    VmaAllocation _allocation {};

  public:
    Buffer() = default;
    Buffer(const VulkanState &state, size_t byte_size,
           vk::BufferUsageFlags usage_flags,
           vk::MemoryPropertyFlags memory_properties);
    Buffer(Buffer &&other) noexcept {
      std::swap(_state, other._state);
      std::swap(_size, other._size);
      std::swap(_usage_flags, other._usage_flags);
      std::swap(_buffer, other._buffer);
      std::swap(_allocation, other._allocation);
    }
    Buffer & operator=(Buffer &&other) noexcept {
      std::swap(_state, other._state);
      std::swap(_size, other._size);
      std::swap(_usage_flags, other._usage_flags);
      std::swap(_buffer, other._buffer);
      std::swap(_allocation, other._allocation);
      return *this;
    }
    virtual ~Buffer() noexcept;

    const VulkanState & state() const noexcept { return *_state; }
    vk::Buffer buffer() const noexcept { return _buffer; }
    VmaAllocation allocation() const noexcept { return _allocation; }

    size_t byte_size() const noexcept { return _size; }
  };

  class HostBuffer : public Buffer {
  private:
    class MappedData {
      friend class HostBuffer;

    private:
      HostBuffer *_buf = nullptr;
      void *_data = nullptr;

    public:
      MappedData(HostBuffer &buf) : _buf(&buf) {}
      ~MappedData() { if (mapped()) { unmap(); } }

      MappedData() noexcept = default;

      void * map() noexcept;
      void unmap() noexcept;
      bool mapped() const noexcept { return _data != nullptr; }
      void * get() noexcept { return _data; }
    };

    MappedData _mapped_data;

  public:
    HostBuffer() noexcept : Buffer(), _mapped_data(*this) {}

    HostBuffer(HostBuffer &&other) noexcept { *this = std::move(other); };
    HostBuffer & operator=(HostBuffer &&other) noexcept;

    HostBuffer(const HostBuffer&) noexcept = delete;
    HostBuffer & operator=(const HostBuffer&) noexcept = delete;

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

    ~HostBuffer() noexcept override {}

    // returns view on CPU memory mapped to GPU memory region
    // modification is slow and forbidden
    // if you want to modify buffer data checkout `copy` method
    std::span<const std::byte> read() noexcept;

    // returns copied data from GPU memory region
    // cheap to modify - do things in this order: copy -> modify -> write
    std::vector<std::byte> copy() noexcept;

    HostBuffer & write(std::span<const std::byte> src);

    template <size_t Extent>
    HostBuffer & write(std::span<const std::byte, Extent> src)
    {
      return write(std::span<const std::byte>(src.data(), src.size()));
    }

    template <typename T, size_t Extent>
    HostBuffer &write(std::span<T, Extent> src) { return write(std::as_bytes(src)); }
  };

  class DeviceBuffer : public Buffer {
  public:
    DeviceBuffer() noexcept = default;
    DeviceBuffer(DeviceBuffer &&) noexcept = default;
    DeviceBuffer &operator=(DeviceBuffer &&) noexcept = default;
    DeviceBuffer(const DeviceBuffer&) noexcept = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) noexcept = delete;

    DeviceBuffer(
      const VulkanState &state, std::size_t size,
      vk::BufferUsageFlags usage_flags,
      vk::MemoryPropertyFlags memory_properties = vk::MemoryPropertyFlags(0))
        : Buffer(state, size, usage_flags,
                 memory_properties | vk::MemoryPropertyFlagBits::eDeviceLocal)
    {
    }

    DeviceBuffer & resize(CommandUnit &command_unit, std::size_t new_size) noexcept;

    DeviceBuffer & write(CommandUnit &command_unit, std::span<const std::byte> src, vk::DeviceSize offset = 0);

    template <size_t Extent>
    DeviceBuffer & write(CommandUnit &command_unit, std::span<const std::byte, Extent> src, vk::DeviceSize offset = 0)
    {
      return write(command_unit, std::span<const std::byte>(src.data(), src.size()), offset);
    }

    template <typename T, size_t Extent>
    DeviceBuffer & write(CommandUnit &command_unit, std::span<T, Extent> src, vk::DeviceSize offset = 0)
    {
      return write(command_unit, std::as_bytes(src), offset);
    }
  };

  class UniformBuffer : public HostBuffer {
  public:
    UniformBuffer() noexcept = default;
    UniformBuffer(UniformBuffer&&) noexcept = default;
    UniformBuffer& operator=(UniformBuffer&&) noexcept = default;
    UniformBuffer(const UniformBuffer&) noexcept = delete;
    UniformBuffer& operator=(const UniformBuffer&) noexcept = delete;

    UniformBuffer(const VulkanState &state, size_t size, vk::BufferUsageFlags usage_flags = {});

    template <typename T, size_t Extent>
    UniformBuffer(const VulkanState &state, std::span<T, Extent> src)
      : UniformBuffer(state, src.size() * sizeof(T))
    {
      ASSERT(src.data());
      write(src);
    }
  };
}
} // namespace mr

#endif // __MR_BUFFER_CORE_HPP_
