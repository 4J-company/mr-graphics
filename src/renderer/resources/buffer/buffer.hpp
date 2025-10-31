#ifndef __MR_BUFFER_HPP_
#define __MR_BUFFER_HPP_

#include <memory>
#include "pch.hpp"

#include <vk_mem_alloc.h>

#include "vulkan_state.hpp"
#include "resources/command_unit/command_unit.hpp"
#include <vulkan/vulkan_core.h>

namespace mr {
inline namespace graphics {
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
    HostBuffer() noexcept : Buffer(), _mapped_data(*this) {}
    HostBuffer(HostBuffer &&) noexcept = default;
    HostBuffer &operator=(HostBuffer &&) noexcept = default;
    HostBuffer(const HostBuffer&) noexcept = delete;
    HostBuffer& operator=(const HostBuffer&) noexcept = delete;

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

  class StorageBuffer : public DeviceBuffer {
  public:
    StorageBuffer() noexcept = default;
    StorageBuffer(StorageBuffer&&) noexcept = default;
    StorageBuffer& operator=(StorageBuffer&&) noexcept = default;
    StorageBuffer(const StorageBuffer&) noexcept = delete;
    StorageBuffer& operator=(const StorageBuffer&) noexcept = delete;

    StorageBuffer(const VulkanState &state, size_t byte_size, vk::BufferUsageFlags usage_flags = {});

    template <typename T, size_t Extent>
    StorageBuffer(const VulkanState &state, std::span<T, Extent> src)
        : StorageBuffer(state, src.size() * sizeof(T))
    {
      ASSERT(src.data());

      CommandUnit command_unit {state};
      command_unit.begin();
      write(command_unit, src);
      command_unit.end();

      UniqueFenceGuard(_state->device(), command_unit.submit(*_state));
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
      CommandUnit command_unit {state};
      command_unit.begin();
      write(command_unit, src);
      command_unit.end();

      UniqueFenceGuard(_state->device(), command_unit.submit(*_state));
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
      ASSERT(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4,
             "Unsupported index element size for IndexBuffer",
             src);

      CommandUnit command_unit(state) ;
      command_unit.begin();
      write(command_unit, src);
      command_unit.end();

      UniqueFenceGuard(_state->device(), command_unit.submit(*_state));

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

  // for VK_EXT_conditional_rendering
  class ConditionalBuffer : public StorageBuffer {
  public:
    ConditionalBuffer() = default;
    ConditionalBuffer(ConditionalBuffer &&) noexcept = default;
    ConditionalBuffer & operator=(ConditionalBuffer &&) noexcept = default;
    ConditionalBuffer(const ConditionalBuffer &) noexcept = delete;
    ConditionalBuffer & operator=(const ConditionalBuffer &) noexcept = delete;
    ConditionalBuffer(const VulkanState &state, size_t byte_size, vk::BufferUsageFlags usage_flags = {})
      : StorageBuffer(state, byte_size, usage_flags | vk::BufferUsageFlagBits::eConditionalRenderingEXT)
    {
    }
  };

  class VectorBuffer : public DeviceBuffer {
  private:
    vk::DeviceSize _current_size;

  public:
    static inline constexpr float resize_coefficient = 1.5;
    static inline constexpr vk::DeviceSize default_initial_byte_size = 1 << 20;

    VectorBuffer() = default;
    VectorBuffer(VectorBuffer &&) noexcept = default;
    VectorBuffer & operator=(VectorBuffer &&) noexcept = default;
    VectorBuffer(const VectorBuffer &) noexcept = delete;
    VectorBuffer & operator=(const VectorBuffer &) noexcept = delete;

    VectorBuffer(const VulkanState &state, vk::BufferUsageFlags usage_flags,
        vk::DeviceSize byte_size = default_initial_byte_size);

    vk::DeviceSize capacity() const noexcept { return DeviceBuffer::_size; }
    vk::DeviceSize size() const noexcept { return _current_size; }

    void resize(vk::DeviceSize new_size) noexcept;

    vk::DeviceSize append_range(CommandUnit &command_unit, std::span<const std::byte> src) noexcept;

    template <size_t Extent>
    vk::DeviceSize append_range(CommandUnit &command_unit, std::span<const std::byte, Extent> src)
    {
      return append_range(command_unit, std::span<const std::byte>(src.data(), src.size()));
    }

    template <typename T, size_t Extent>
    const vk::DeviceSize append_range(CommandUnit &command_unit, std::span<T, Extent> src) noexcept
    {
      return append_range(command_unit, std::as_bytes(src));
    }
  };

  class VertexVectorBuffer : public VectorBuffer {
  public:
    VertexVectorBuffer() = default;
    VertexVectorBuffer(VertexVectorBuffer &&) noexcept = default;
    VertexVectorBuffer & operator=(VertexVectorBuffer &&) noexcept = default;
    VertexVectorBuffer(const VertexVectorBuffer &) noexcept = delete;
    VertexVectorBuffer & operator=(const VertexVectorBuffer &) noexcept = delete;
    VertexVectorBuffer(const VulkanState &state,
        vk::DeviceSize initial_byte_size = VectorBuffer::default_initial_byte_size)
      : VectorBuffer(state,
          vk::BufferUsageFlagBits::eVertexBuffer |
            vk::BufferUsageFlagBits::eTransferDst,
          initial_byte_size)
    {
    }
  };

  class IndexVectorBuffer : public VectorBuffer {
  public:
    IndexVectorBuffer() = default;
    IndexVectorBuffer(IndexVectorBuffer &&) noexcept = default;
    IndexVectorBuffer & operator=(IndexVectorBuffer &&) noexcept = default;
    IndexVectorBuffer(const IndexVectorBuffer &) noexcept = delete;
    IndexVectorBuffer & operator=(const IndexVectorBuffer &) noexcept = delete;
    IndexVectorBuffer(const VulkanState &state,
        vk::DeviceSize initial_byte_size = VectorBuffer::default_initial_byte_size)
      : VectorBuffer(state,
          vk::BufferUsageFlagBits::eIndexBuffer |
            vk::BufferUsageFlagBits::eTransferDst,
          initial_byte_size)
    {
    }
  };

  /* VmaVirtualBlock is not resizeable. This is a workaround.
   * This structure handles array of VmaVirtualBlock
   * If allocation of requested size is not possible -
   *     create another VmaVirtualBlock larger than last one
   */
  class DeviceHeapAllocator {
  private:
    // individual allocation wrapper
    struct Allocation {
      VmaVirtualAllocation allocation;
      vk::DeviceSize byte_size;
      uint32_t block_number;
    };

    // wrapper around single VmaVirtualBlock
    class AllocationBlock {
      friend class DeviceHeapAllocator;

    private:
      VmaVirtualBlock _virtual_block = nullptr;
      vk::DeviceSize _size = 0;
      vk::DeviceSize _offset = 0;
      std::atomic<uint32_t> _allocation_number = 0;
      uint32_t _block_number = 0;
      // mutex for concurrent work with virtual block.
      // Maybe it is unneccessary, but now I can not prove what VmaVirtualBlock functions is thread safe)
      std::mutex _mutex;

    public:
      AllocationBlock(vk::DeviceSize size, vk::DeviceSize offset, uint32_t block_number) noexcept;
      ~AllocationBlock() noexcept;

      // these methods aren't thread safe
      AllocationBlock & operator=(AllocationBlock &&other) noexcept;
      AllocationBlock(AllocationBlock &&other) noexcept { *this = std::move(other); }

      std::optional<std::pair<vk::DeviceSize, Allocation>> allocate(vk::DeviceSize allocation_size,
                                                                  uint32_t alignment) noexcept;
      void deallocate(Allocation &allocation) noexcept;
    };

  public:
    struct AllocationInfo {
      vk::DeviceSize offset;
      bool resized = false;
    };

  private:
    std::atomic<uint32_t> _size = 0;
    uint32_t _alignment = 16;

    std::shared_mutex _allocations_mutex;
    tbb::concurrent_unordered_map<vk::DeviceSize, Allocation> _allocations;

    std::mutex _add_block_mutex; // mutex for correct execution of 'add_block' function
    // we use TBB vector for itteration over this while it can be resized (so memory can be reallocated)
    tbb::concurrent_vector<AllocationBlock> _blocks;

  public:
    // alignment must be pow of 2
    DeviceHeapAllocator(vk::DeviceSize start_byte_size = 1'000'000, vk::DeviceSize alignment = 16);

    // these methods aren't thread safe
    DeviceHeapAllocator(DeviceHeapAllocator &&other) noexcept { *this = std::move(other); };
    DeviceHeapAllocator & operator=(DeviceHeapAllocator &&) noexcept;

    // size must be aligned by alignment parameter
    AllocationInfo allocate(vk::DeviceSize size) noexcept;
    // offset if offset to allocation returned by allocate
    void deallocate(vk::DeviceSize offset) noexcept;

    vk::DeviceSize size() const noexcept { return _size; }
    uint32_t alignment() const noexcept { return _alignment; }

  private:
    AllocationBlock & add_block(vk::DeviceSize allocation_size = 0) noexcept;
  };

  class HeapBuffer {
  private:
    VectorBuffer _buffer;
    DeviceHeapAllocator _heap;

  public:
    static inline constexpr auto default_initial_byte_size = VectorBuffer::default_initial_byte_size;
    static inline constexpr auto default_alignment = 16;

    HeapBuffer() = default;
    HeapBuffer(HeapBuffer &&) noexcept = default;
    HeapBuffer & operator=(HeapBuffer &&) noexcept = default;
    HeapBuffer(const HeapBuffer &) noexcept = delete;
    HeapBuffer & operator=(const HeapBuffer &) noexcept = delete;

    HeapBuffer(const VulkanState &state, vk::BufferUsageFlags usage_flags,
               vk::DeviceSize start_byte_size = HeapBuffer::default_initial_byte_size,
               vk::DeviceSize alignment = HeapBuffer::default_alignment);

    // return offset in buffer
    vk::DeviceSize allocate(vk::DeviceSize size) noexcept;
    void free(vk::DeviceSize offset) noexcept;

    template <typename T, size_t Extent>
    void write(CommandUnit &command_unit, std::span<T, Extent> src, vk::DeviceSize offset = 0)
    {
      _buffer.write(command_unit, src, offset);
    }

    // return offset in buffer
    template <typename T, size_t Extent>
    const vk::DeviceSize allocate_and_write(CommandUnit &command_unit, std::span<T, Extent> src) noexcept
    {
      ASSERT(src.data());
      return allocate_and_write(command_unit, std::as_bytes(src));
    }
    // return offset in buffer
    vk::DeviceSize allocate_and_write(CommandUnit &command_unit, std::span<const std::byte> src) noexcept;

    vk::DeviceSize byte_size() const noexcept { return _buffer.byte_size(); }
    vk::Buffer buffer() const noexcept { return _buffer.buffer(); }
  };

  class VertexHeapBuffer : public HeapBuffer {
  public:
    VertexHeapBuffer() = default;
    VertexHeapBuffer(VertexHeapBuffer &&) noexcept = default;
    VertexHeapBuffer & operator=(VertexHeapBuffer &&) noexcept = default;
    VertexHeapBuffer(const VertexHeapBuffer &) noexcept = delete;
    VertexHeapBuffer & operator=(const VertexHeapBuffer &) noexcept = delete;

    VertexHeapBuffer(const VulkanState &state,
        vk::DeviceSize start_byte_size = HeapBuffer::default_initial_byte_size,
        vk::DeviceSize alignment = HeapBuffer::default_alignment)
      : HeapBuffer(state,
          vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
          start_byte_size,
          alignment)
    {
    }
  };

  class IndexHeapBuffer : public HeapBuffer {
  public:
    IndexHeapBuffer() = default;
    IndexHeapBuffer(IndexHeapBuffer &&) noexcept = default;
    IndexHeapBuffer & operator=(IndexHeapBuffer &&) noexcept = default;
    IndexHeapBuffer(const IndexHeapBuffer &) noexcept = delete;
    IndexHeapBuffer & operator=(const IndexHeapBuffer &&) noexcept = delete;

    IndexHeapBuffer(const VulkanState &state,
        vk::DeviceSize start_byte_size = HeapBuffer::default_initial_byte_size,
        vk::DeviceSize alignment = HeapBuffer::default_alignment)
      : HeapBuffer(state,
          vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
          start_byte_size,
          alignment)
    {
    }
  };

  struct BufferRegion {
    const VulkanState &state {};
    vk::Buffer buffer {};
    vk::DeviceSize offset {};
    vk::DeviceSize size {};

    BufferRegion() noexcept = default;
    BufferRegion(BufferRegion &&) noexcept = default;
    BufferRegion & operator=(BufferRegion &&) noexcept = delete;
    BufferRegion(const BufferRegion &) noexcept = default;
    BufferRegion & operator=(const BufferRegion &) noexcept = delete;

    template <std::derived_from<Buffer> BufferT>
    BufferRegion(const BufferT &buf, vk::DeviceSize o = 0, vk::DeviceSize s = 0)
    : state(buf.state())
    , buffer(buf.buffer())
    , offset(o)
    , size(s == 0 ? buf.byte_size() - o : s)
    {
    }
  };

  void bufcopy(CommandUnit &command_unit, BufferRegion src, BufferRegion dst);
}
} // namespace mr

#endif // __MR_BUFFER_HPP_
