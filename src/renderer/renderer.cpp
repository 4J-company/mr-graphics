#include "renderer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "window_context.hpp"

// ter::application class defualt constructor (initializes vulkan instance, device ...)
ter::Application::Application() {}

ter::Application::~Application() {}

[[nodiscard]] std::unique_ptr<ter::Buffer> ter::Application::create_buffer() const
{
  return std::make_unique<Buffer>();
}

[[nodiscard]] std::unique_ptr<ter::CommandUnit> ter::Application::create_command_unit() const
{
  return std::make_unique<CommandUnit>();
}

[[nodiscard]] std::unique_ptr<wnd::Window> ter::Application::create_window(size_t width, size_t height) const
{
  return std::make_unique<wnd::Window>(state);
}
