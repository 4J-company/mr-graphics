#if !defined(__renderer_hpp_)
#define __renderer_hpp_

#include "pch.hpp"

#include "resources/resources.hpp"
#include "window_context/window_context.hpp"

namespace ter {
class application : window_system::application {
  friend class window_context;

private:
  vk::Instance _instance;
  vk::Device _device;
  vk::PhysicalDevice _phys_device;
#if __DEBUG
  vk::DebugUtilsMessengerEXT _dbg_messenger;
#endif

public:
  application();
  ~application();

  [[nodiscard]] std::unique_ptr<command_unit> create_command_unit();
};
} // namespace ter

#endif // __renderer_hpp_
