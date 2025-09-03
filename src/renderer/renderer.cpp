#include "renderer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "window/render_context.hpp"
#include "manager/manager.hpp"

// mr::Application class defualt constructor (initializes vulkan instance, device ...)
mr::Application::Application()
{
  while (vkfw::init() != vkfw::Result::eSuccess)
    ;
  // _state._init();
}

// destructor
mr::Application::~Application()
{
  // _state._deinit();
}

[[nodiscard]] std::unique_ptr<mr::HostBuffer>
mr::Application::create_host_buffer() const
{
  return std::make_unique<HostBuffer>();
}

[[nodiscard]] std::unique_ptr<mr::DeviceBuffer>
mr::Application::create_device_buffer() const
{
  return std::make_unique<DeviceBuffer>();
}

[[nodiscard]] std::unique_ptr<mr::CommandUnit>
mr::Application::create_command_unit() const
{
  return std::make_unique<CommandUnit>();
}

[[nodiscard]] std::unique_ptr<mr::Window>
mr::Application::create_window(Extent extent)
{
  return std::make_unique<Window>(&_state, extent);
}
