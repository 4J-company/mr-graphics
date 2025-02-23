#ifndef __MR_BUFFER_HPP_
#define __MR_BUFFER_HPP_

#include "pch.hpp"


#include "resources/command_unit/command_unit.hpp"
#include "vulkan_state.hpp"

namespace mr {
  class Buffer {
    public:
      /**
 * @brief Default constructor for the Buffer class.
 *
 * Constructs a Buffer object with its members initialized to their default values.
 */
Buffer() = default;
      Buffer(const VulkanState &state, size_t byte_size,
             vk::BufferUsageFlags usage_flag,
             vk::MemoryPropertyFlags memory_properties);
      /**
 * @brief Move constructs a Buffer instance.
 *
 * Transfers ownership of the internal resources from the source Buffer, leaving it in a valid but unspecified state.
 */
Buffer(Buffer &&) = default;
      /**
 * @brief Move assignment operator for Buffer.
 *
 * Transfers the ownership of resources from the source Buffer to this instance,
 * leaving the source in a valid but unspecified state.
 *
 * @return Reference to this Buffer.
 */
Buffer &operator=(Buffer &&) = default;
      /**
 * @brief Virtual destructor for the Buffer class.
 *
 * Provides a default implementation ensuring that derived classes are properly destroyed.
 */
virtual ~Buffer() = default;

      void resize(size_t byte_size);

      static uint find_memory_type(const VulkanState &state, uint filter,
                                   vk::MemoryPropertyFlags properties) noexcept;

      vk::Buffer buffer() const noexcept { return _buffer.get(); }

      /**
 * @brief Retrieves the size of the buffer in bytes.
 *
 * This method returns the current allocation size of the buffer, measured in bytes.
 *
 * @return size_t The size of the buffer in bytes.
 */
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
      /**
 * @brief Default constructor for HostBuffer.
 *
 * Constructs an uninitialized HostBuffer instance. The underlying Vulkan host buffer is not set up until explicitly initialized, so ensure you configure it appropriately before use.
 */
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
      /**
       * @brief Writes data from a source span into the host buffer.
       *
       * Maps the buffer's device memory, copies the contents of the provided span into this memory,
       * and then unmaps the memory. The size of the data (computed as src.size() * sizeof(T)) must not exceed
       * the buffer's allocated size.
       *
       * @tparam T The type of elements in the source span.
       * @tparam Extent The compile-time extent of the span.
       * @param src A span containing the data to write.
       * @return A reference to the HostBuffer, enabling method chaining.
       *
       * @note This function asserts if the data size exceeds the buffer's capacity or if the internal state
       * is uninitialized.
       */
      HostBuffer &write(std::span<T, Extent> src)
      {
        // std::span<const T, Extent> cannot be constructed from non-const

        size_t byte_size = src.size() * sizeof(T);
        assert(byte_size <= _size);

        assert(_state != nullptr);
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
      /**
       * @brief Uploads data to the device-local buffer via a staged copy.
       *
       * This method writes the provided data into the device-local buffer by staging the transfer
       * through a temporary host-visible buffer. It verifies that the data fits within the buffer's
       * capacity, writes the data to the temporary buffer, and then issues a command to copy the data
       * to the device-local buffer. The method waits for the copy command to complete before returning.
       *
       * @tparam T The type of the elements in the input span.
       * @tparam Extent The extent of the span; can be dynamic.
       * @param src The span containing the data to write.
       * @return DeviceBuffer& A reference to the current device buffer instance for method chaining.
       *
       * @note This method uses assertions to ensure the data size does not exceed the buffer's capacity
       *       and that the internal state is properly initialized.
       */
      DeviceBuffer &write(std::span<T, Extent> src)
      {
        // std::span<const T, Extent> cannot be constructed from non-const

        size_t byte_size = src.size() * sizeof(T);
        assert(byte_size <= _size);

        assert(_state != nullptr);
        auto buf =
          HostBuffer(*_state, _size, vk::BufferUsageFlagBits::eTransferSrc);
        buf.write(src);

        vk::BufferCopy buffer_copy {.size = byte_size};

        static CommandUnit command_unit(*_state);
        command_unit.begin();
        command_unit->copyBuffer(buf.buffer(), _buffer.get(), {buffer_copy});
        command_unit.end();

        auto [bufs, bufs_number] = command_unit.submit_info();
        vk::SubmitInfo submit_info {
          .commandBufferCount = bufs_number,
          .pCommandBuffers = bufs,
        };
        auto fence = _state->device().createFence({}).value;
        _state->queue().submit(submit_info, fence);
        _state->device().waitForFences({fence}, VK_TRUE, UINT64_MAX);

        return *this;
      }
  };

  class UniformBuffer : public HostBuffer {
    public:
      /**
 * @brief Default constructs a UniformBuffer.
 *
 * Constructs an empty UniformBuffer object. This constructor initializes the buffer without
 * any allocated uniform data. Use the appropriate write() method or alternative constructors
 * to allocate and populate the buffer as needed.
 */
UniformBuffer() = default;

      /**
       * @brief Constructs a UniformBuffer with a specified size.
       *
       * Initializes a uniform buffer backed by host-visible memory and configured with
       * the uniform buffer usage flag (vk::BufferUsageFlagBits::eUniformBuffer). The provided
       * buffer size determines the allocated byte capacity. The necessary Vulkan state is
       * accessed internally.
       *
       * @param size The size of the uniform buffer in bytes.
       */
      UniformBuffer(const VulkanState &state, size_t size)
          : HostBuffer(state, size, vk::BufferUsageFlagBits::eUniformBuffer)
      {
      }

      template <typename T, size_t Extent>
      /**
       * @brief Constructs and initializes a UniformBuffer with initial data.
       *
       * Creates a uniform buffer using the provided Vulkan state, sizing it to accommodate
       * the total byte count of the input span (computed as the number of elements multiplied
       * by the size of each element). The buffer is immediately populated with the provided data.
       *
       * @param src Data span containing the initial uniform data.
       */
      UniformBuffer(const VulkanState &state, std::span<T, Extent> src)
          : UniformBuffer(state, src.size() * sizeof(T))
      {
        assert(src.data());
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
      /**
       * @brief Constructs a VertexBuffer from provided vertex data.
       *
       * This constructor initializes a vertex buffer sized to accommodate the vertex data in the given span. 
       * The buffer size is determined by multiplying the number of elements by the size of each element. 
       * An assertion ensures that the source data pointer is valid before the data is written into the buffer.
       *
       * @param src A span containing the vertex data to be copied into the buffer.
       */
      VertexBuffer(const VulkanState &state, std::span<T, Extent> src)
          : VertexBuffer(state, src.size() * sizeof(T))
      {
        assert(src.data());
        write(src);
      }
  };

  class IndexBuffer : public DeviceBuffer {
    private:
      vk::IndexType _index_type;
      std::size_t _element_count;

    public:
      /**
 * @brief Default constructor for IndexBuffer.
 *
 * Initializes a new instance of IndexBuffer with default settings. No Vulkan resources or data are allocated, and the buffer requires explicit initialization before use.
 */
IndexBuffer() = default;

      /**
       * @brief Constructs an index buffer with the specified size.
       *
       * Initializes a device-local index buffer designed for index data storage by invoking the
       * base class constructor with usage flags set for both index buffering and transfer destination operations.
       *
       * @param state The Vulkan state used for allocating and managing GPU resources.
       * @param byte_size The size in bytes to allocate for the buffer.
       */
      IndexBuffer(const VulkanState &state, size_t byte_size)
          : DeviceBuffer(state, byte_size,
                         vk::BufferUsageFlagBits::eIndexBuffer |
                           vk::BufferUsageFlagBits::eTransferDst)
      {
      }

      IndexBuffer(IndexBuffer &&) noexcept = default;
      IndexBuffer &operator=(IndexBuffer &&) noexcept = default;

      template <typename T, size_t Extent>
      /**
       * @brief Constructs an IndexBuffer from a span of index data.
       *
       * Initializes the index buffer by copying the provided index data into the buffer,
       * recording the number of indices, and determining the appropriate Vulkan index type based on
       * the size of the index elements:
       * - 4 bytes: vk::IndexType::eUint32
       * - 2 bytes: vk::IndexType::eUint16
       * - 1 byte: vk::IndexType::eUint8KHR
       *
       * @param src Span containing the index data to populate the buffer.
       */
      IndexBuffer(const VulkanState &state, std::span<T, Extent> src)
          : IndexBuffer(state, src.size() * sizeof(T))
      {
        assert(src.data());
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

      /**
 * @brief Returns the number of index elements stored in the buffer.
 *
 * This method retrieves the count of indices that the buffer currently holds,
 * as determined during initialization.
 *
 * @return std::size_t The total number of index elements.
 */
std::size_t   element_count() const noexcept { return _element_count; }
      /**
 * @brief Retrieves the Vulkan index type of the index buffer.
 *
 * This function returns the index type that specifies how the index data is interpreted,
 * such as whether indices are 16-bit or 32-bit.
 *
 * @return vk::IndexType The current index type of the buffer.
 */
vk::IndexType index_type() const noexcept { return _index_type; }
  };
} // namespace mr

#endif // __MR_BUFFER_HPP_
