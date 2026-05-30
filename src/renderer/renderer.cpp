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
mr::Application::create_render_context(Extent extent, RenderContextConfig config)
{
  return std::make_unique<RenderContext>(&_state, extent, config);
}

void mr::Application::start_render_loop(RenderContext &render_context, SceneHandle scene, WindowHandle window,
                                        std::optional<std::reference_wrapper<std::ostream>> stat_log_stream,
                                        std::optional<uint32_t> frame_number) const noexcept
{
  std::atomic_bool render_cycle_end = false;
  std::jthread render_thread {
    [&](std::stop_token stop_token) {
      uint32_t frame = 0;
      while (not stop_token.stop_requested()) {
        if (frame_number.has_value() && frame++ >= frame_number.value()) {
          break;
        }
        window->update_state();
        scene->update(std::optional(std::reference_wrapper(window->input_state())));
        render_context.render(scene, *window);

        if (stat_log_stream.has_value()) {
          render_context.prev_stat().write_to_json(stat_log_stream.value().get());
          // render_context.stat().write_to_json(stat_log_stream.value().get());
        }
      }
      render_cycle_end = true;
    }
  };

  // TMP theme
  while (not window->window().shouldClose().value && not render_cycle_end) {
    vkfw::pollEvents();
  }
}


void mr::Application::render_frames(RenderContext &render_context,
                                    SceneHandle scene,
                                    FileWriterHandle file_writer,
                                    std::fs::path dst_dir,
                                    std::string_view filename_prefix,
                                    std::optional<uint32_t> frames) const noexcept
{
  ASSERT(frames > 0);
  ASSERT(not filename_prefix.empty());

  std::fs::create_directory(dst_dir);

  auto base_filename = std::format("{}/{}", dst_dir.string().c_str(), filename_prefix);
  file_writer->filename(base_filename);
  for (uint32_t i = 0; frames.has_value() ? (i < frames) : true; i++) {
    if (frames > 1) {
      auto filename = std::format("{}{}", base_filename, i);
      file_writer->filename(filename);
    }

    scene->update();
    render_context.render(scene, *file_writer);
  }
}
