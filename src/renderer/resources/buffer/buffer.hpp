#ifndef __MR_BUFFER_HPP_
#define __MR_BUFFER_HPP_

#include <memory>
#include "pch.hpp"

#include <vk_mem_alloc.h>

#include "vulkan_state.hpp"
#include <vulkan/vulkan_core.h>

namespace mr {
inline namespace graphics {
  class Buffer {
  protected:
    const VulkanState *_state = nullptr;

    size_t _size = 0;
    vk::Buffer _buffer {};
    VmaAllocation _allocation {};

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

    // void resize(size_t byte_size);

    static uint find_memory_type(const VulkanState &state, uint filter,
                                 vk::MemoryPropertyFlags properties) noexcept;

    vk::Buffer buffer() const noexcept { return _buffer; }

    size_t byte_size() const noexcept { return _size; }

  protected:
    static std::pair<vk::Buffer, VmaAllocation> create_buffer(const VulkanState &state,
                                                              vk::BufferUsageFlags usage_flags,
                                                              vk::MemoryPropertyFlags memory_properties,
                                                              size_t byte_size);
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

    DeviceBuffer & write(std::span<const std::byte> src, VkDeviceSize offset = 0);

    template <size_t Extent>
    DeviceBuffer & write(std::span<const std::byte, Extent> src, VkDeviceSize offset = 0)
    {
      return write(std::span<const std::byte>(src.data(), src.size()));
    }

    template <typename T, size_t Extent>
    DeviceBuffer & write(std::span<T, Extent> src, VkDeviceSize offset = 0) { return write(std::as_bytes(src), offset); }
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

  class VectorBuffer : public DeviceBuffer {
  private:
    vk::BufferUsageFlags _usage_flags {};
    VkDeviceSize _current_size;

  public:
    VectorBuffer() = default;

    VectorBuffer(const VulkanState &state, vk::BufferUsageFlags usage_flags, VkDeviceSize start_byte_size = 1'000'000);

    VectorBuffer(VectorBuffer &&) noexcept = default;
    VectorBuffer & operator=(VectorBuffer &&) noexcept = default;

    template <typename T, size_t Extent>
    const VkDeviceSize push_data_back(std::span<T, Extent> src) noexcept
    {
      ASSERT(src.data());
      return push_data_back(std::as_bytes(src));
    }
    VkDeviceSize push_data_back(std::span<const std::byte> src) noexcept;

    void resize(VkDeviceSize new_size) noexcept;

  private:
    void recreate_buffer(VkDeviceSize new_size) noexcept;
  };

  class VertexVectorBuffer : public VectorBuffer {
  public:
    VertexVectorBuffer() = default;

    VertexVectorBuffer(const VulkanState &state, VkDeviceSize start_byte_size = 1'000'000);

    VertexVectorBuffer(VertexVectorBuffer &&) noexcept = default;
    VertexVectorBuffer & operator=(VertexVectorBuffer &&) noexcept = default;
  };

  class DeviceHeapAllocator {
  private:
    struct Allocation {
      VmaVirtualAllocation allocation;
      VkDeviceSize byte_size;
      uint32_t block_number;
    };

    class AllocationBlock {
      friend class DeviceHeapAllocator;

    private:
      VmaVirtualBlock _virtual_block = nullptr;
      VkDeviceSize _size = 0;
      VkDeviceSize _offset = 0;
      std::atomic<uint32_t> _allocation_number = 0;
      uint32_t _block_number = 0;
      // mutex for concurrent work with virtual block.
      // Maybe it is unneccessary, but now I can not prove what VmaVirtualBlock functions is thread safe)
      std::mutex _mutex;

    public:
      AllocationBlock(VkDeviceSize size, VkDeviceSize offset, uint32_t block_number) noexcept;
      ~AllocationBlock() noexcept;

      // these methods aren't thread safe
      AllocationBlock & operator=(AllocationBlock &&other) noexcept;
      AllocationBlock(AllocationBlock &&other) noexcept { *this = std::move(other); }

      std::optional<std::pair<VkDeviceSize, Allocation>> allocate(VkDeviceSize allocation_size,
                                                                  uint32_t alignment) noexcept;
      void deallocate(Allocation &allocation) noexcept;
    };

  public:
    struct AllocInfo {
      VkDeviceSize offset;
      bool resized = false;
    };

  private:
    std::atomic<uint32_t> _size = 0;
    uint32_t _alignment = 16;

    std::mutex _allocations_mutex;
    boost::unordered_map<VkDeviceSize, Allocation> _allocations;

    std::mutex _add_block_mutex; // mutex for correct execution of 'add_block' function
    // we use TBB vector for itteration over this while it can be resized (so memory can be reallocated)
    tbb::concurrent_vector<AllocationBlock> _blocks;

  public:
    // alignment must be pow of 2
    DeviceHeapAllocator(VkDeviceSize start_byte_size = 1'000'000, VkDeviceSize alignment = 16);

    // these methods aren't thread safe
    DeviceHeapAllocator(DeviceHeapAllocator &&other) noexcept { *this = std::move(other); };
    DeviceHeapAllocator & operator=(DeviceHeapAllocator &&) noexcept;

    // size must be aligned by alignment parameter
    AllocInfo allocate(VkDeviceSize size) noexcept;
    // offset if offset to allocation returned by allocate
    void deallocate(VkDeviceSize offset) noexcept;

    VkDeviceSize size() const noexcept { return _size; }
    uint32_t alignment() const noexcept { return _alignment; }

  private:
    AllocationBlock & add_block(VkDeviceSize allocation_size = 0) noexcept;
  };

  class HeapBuffer {
  private:
    VectorBuffer _buffer;
    DeviceHeapAllocator _heap;

  public:
    HeapBuffer() = default;

    HeapBuffer(const VulkanState &state, vk::BufferUsageFlags usage_flags,
               VkDeviceSize start_byte_size = 1'000'000, VkDeviceSize alignment = 16);

    HeapBuffer(HeapBuffer &&) noexcept = default;
    HeapBuffer & operator=(HeapBuffer &&) noexcept = default;

    // return offset in buffer
    VkDeviceSize allocate(VkDeviceSize size) noexcept;
    void free(VkDeviceSize offset) noexcept;

    template <typename T, size_t Extent>
    void write(std::span<T, Extent> src, VkDeviceSize offset = 0) { _buffer.write(src, offset); }

    // return offset in buffer
    template <typename T, size_t Extent>
    const VkDeviceSize allocate_and_write(std::span<T, Extent> src) noexcept
    {
      ASSERT(src.data());
      return allocate_and_write(std::as_bytes(src));
    }
    // return offset in buffer
    VkDeviceSize allocate_and_write(std::span<const std::byte> src) noexcept;

    VkDeviceSize byte_size() const noexcept { return _buffer.byte_size(); }
    vk::Buffer buffer() const noexcept { return _buffer.buffer(); }
  };

  class VertexHeapBuffer : public HeapBuffer {
  public:
    VertexHeapBuffer() = default;

    VertexHeapBuffer(const VulkanState &state, VkDeviceSize start_byte_size = 1'000'000, VkDeviceSize alignment = 16);

    VertexHeapBuffer(VertexHeapBuffer &&) noexcept = default;
    VertexHeapBuffer & operator=(VertexHeapBuffer &&) noexcept = default;
  };

  class IndexHeapBuffer : public HeapBuffer {
  public:
    IndexHeapBuffer() = default;

    IndexHeapBuffer(const VulkanState &state, VkDeviceSize start_byte_size = 1'000'000, VkDeviceSize alignment = 16);

    IndexHeapBuffer(IndexHeapBuffer &&) noexcept = default;
    IndexHeapBuffer & operator=(IndexHeapBuffer &&) noexcept = default;
  };


}
} // namespace mr

#endif // __MR_BUFFER_HPP_
