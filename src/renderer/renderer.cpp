#include "renderer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "window/render_context.hpp"

// mr::Application class defualt constructor (initializes vulkan instance, device ...)
mr::Application::Application()
{
}

// destructor
mr::Application::~Application() {}

[[nodiscard]] std::unique_ptr<mr::RenderContext> mr::Application::create_render_context(Extent extent)
{
  return std::make_unique<RenderContext>(&_state, extent);
}

void mr::Application::start_render_loop(RenderContext &render_context, SceneHandle scene,
                                                                       WindowHandle window) const noexcept
{
  std::jthread render_thread {
    [&](std::stop_token stop_token) {
      while (not stop_token.stop_requested()) {
        window->update_state();
        scene->update(window->input_state());
        render_context.render(scene, *window);
      }
    }
  };

  // TMP theme
  while (not window->window().shouldClose().value) {
    vkfw::pollEvents();
  }
}


void mr::Application::render_frame(RenderContext &render_context, SceneHandle scene,
                                                                  FileWriterHandle file_writer) const noexcept
{
  render_context.render(scene, *file_writer);
}