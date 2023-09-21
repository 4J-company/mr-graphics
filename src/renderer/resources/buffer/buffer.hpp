#if !defined(__buffer_hpp_)
  #define __buffer_hpp_

  #include "pch.hpp"


#include "vulkan_application.hpp"

namespace mr
{
  class Buffer
  {
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
    Buffer(const VulkanState &state, size_t size, vk::BufferUsageFlags usage_flag, vk::MemoryPropertyFlags memory_properties);

    // resize
    void resize(size_t size);

    template<typename type>
    void write(const VulkanState &state, type *data)
    {
      auto ptr = state.device().mapMemory(_memory.get(), 0, _size).value;
      memcpy(ptr, data, _size);
      state.device().unmapMemory(_memory.get());
    }
                                                                  
    // find memory tppe
    static uint find_memory_type(const VulkanState &state, uint filter, vk::MemoryPropertyFlags properties);

    const vk::Buffer buffer() { return _buffer.get(); }
  };
} // namespace mr

#endif // __buffer_hpp_
