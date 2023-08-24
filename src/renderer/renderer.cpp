#include "renderer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "window_context.hpp"

// mr::application class defualt constructor (initializes vulkan instance, device ...)
mr::Application::Application() {}

mr::Application::~Application() {}

[[nodiscard]] std::unique_ptr<mr::Buffer> mr::Application::create_buffer() const { return std::make_unique<Buffer>(); }

[[nodiscard]] std::unique_ptr<mr::CommandUnit> mr::Application::create_command_unit() const
{
  return std::make_unique<CommandUnit>();
}

[[nodiscard]] std::unique_ptr<mr::Window> mr::Application::create_window(size_t width, size_t height) const
{
  return std::make_unique<Window>(state);
}
