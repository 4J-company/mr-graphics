#if !defined(__command_unit_hpp_)
  #define __command_unit_hpp_

  #include "pch.hpp"

  #include "vulkan_application.hpp"

namespace mr
{
  class CommandUnit
  {
  private:
    vk::CommandPool _cmd_pool;
    vk::CommandBuffer _cmd_buffer;

  public:
    CommandUnit() = default;
    ~CommandUnit();

    CommandUnit(const VulkanState &state);

    // move semantics
    CommandUnit(CommandUnit &&other) noexcept = default;
    CommandUnit &operator=(CommandUnit &&other) noexcept = default;

    void begin();
    void end();

    std::tuple<vk::CommandBuffer *, uint> submit_info() { return {&_cmd_buffer, 1}; }

    vk::CommandBuffer *operator->() { return &_cmd_buffer; }
  };
} // namespace mr

#endif // __command_unit_hpp_
