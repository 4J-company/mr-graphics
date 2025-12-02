#include "renderer.hpp"
#include <filesystem>
#include <format>
#include "resources/command_unit/command_unit.hpp"
#include "window/render_context.hpp"
#include <string_view>

// mr::Application class defualt constructor (initializes vulkan instance, device ...)
mr::Application::Application(bool init_vkfw) : _state(init_vkfw)
{
}

// destructor
mr::Application::~Application() {}

[[nodiscard]] std::unique_ptr<mr::RenderContext>
mr::Application::create_render_context(Extent extent, RenderOptions options)
{
  return std::make_unique<RenderContext>(&_state, extent, options);
}

void mr::Application::start_render_loop(RenderContext &render_context, SceneHandle scene,
                                                                       WindowHandle window) const noexcept
{
  std::jthread render_thread {
    [&](std::stop_token stop_token) {
      while (not stop_token.stop_requested()) {
        window->update_state();
        scene->update(std::optional(std::reference_wrapper(window->input_state())));
        render_context.render(scene, *window);
        // render_context.stat().write_to_json(std::cout);
        // std::println();
      }
    }
  };

  // TMP theme
  while (not window->window().shouldClose().value) {
    vkfw::pollEvents();
  }
}


void mr::Application::render_frames(RenderContext &render_context,
                                    SceneHandle scene,
                                    FileWriterHandle file_writer,
                                    std::fs::path dst_dir,
                                    std::string_view filename_prefix,
                                    uint32_t frames) const noexcept
{
  ASSERT(frames > 0);
  ASSERT(not filename_prefix.empty());

  std::fs::create_directory(dst_dir);

  auto base_filename = std::format("{}/{}", dst_dir.c_str(), filename_prefix);
  file_writer->filename(base_filename);
  for (uint32_t i = 0; i < frames; i++) {
    if (frames > 1) {
      auto filename = std::format("{}{}", base_filename, i);
      file_writer->filename(filename);
    }

    scene->update();
    render_context.render(scene, *file_writer);
  }
}
