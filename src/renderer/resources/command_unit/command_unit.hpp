#if !defined(__command_unit_hpp_)
  #define __command_unit_hpp_

  #include "pch.hpp"

namespace ter
{
  class CommandUnit
  {
    friend class Application;

  private:
    vk::CommandPool _cmd_pool;
    std::array<vk::CommandBuffer, 3> _cmd_buffer;

  public:
    CommandUnit() = default;
    ~CommandUnit();

    // move semantics
    CommandUnit(CommandUnit &&other) noexcept = default;
    CommandUnit &operator=(CommandUnit &&other) noexcept = default;

    // record
    void record(/* callable, args[] */);
  };
} // namespace ter

#endif // __command_unit_hpp_
