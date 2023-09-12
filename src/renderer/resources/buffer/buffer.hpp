#if !defined(__buffer_hpp_)
  #define __buffer_hpp_

  #include "pch.hpp"

namespace mr
{
  class Buffer
  {
  private:
    void *_host_data;
    size_t _size;

    vk::UniqueBuffer _buffer;
    vk::BufferUsageFlags _usage_flags;

    // VMA data
    // VmaAllocation _allocation;
    // VmaMemoryUsage _memory_usage;

  public:
    Buffer() = default;
    Buffer(size_t size);
    ~Buffer();

    // resize
    void resize(size_t size);

    // find memory tppe
    static uint32_t find_memory_type(uint32_t filter, vk::MemoryPropertyFlags properties);
  };
} // namespace mr

#endif // __buffer_hpp_
