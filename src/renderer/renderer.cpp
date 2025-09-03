#include "renderer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "window/render_context.hpp"
#include "manager/manager.hpp"

// mr::Application class defualt constructor (initializes vulkan instance, device ...)
mr::Application::Application()
{
  while (vkfw::init() != vkfw::Result::eSuccess) {}
}

// destructor
mr::Application::~Application() {}

[[nodiscard]] std::unique_ptr<mr::RenderContext> mr::Application::create_render_context(Extent extent)
{
  return std::make_unique<RenderContext>(&_state, extent);
}

void mr::Application::start_render_loop(RenderContext &render_context, const SceneHandle scene,
                                                                       WindowHandle window) const noexcept
{
  std::jthread render_thread {
    [&](std::stop_token stop_token) {
      while (not stop_token.stop_requested()) {
        render_context.render(scene, window);
      }
    }
  };

  // TMP theme
  while (not window->window().shouldClose().value) {
    vkfw::pollEvents();
  }
}