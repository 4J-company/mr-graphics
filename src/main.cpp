#include <print>
#include <system.hpp>
#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"
#include "render_options/render_options.hpp"

int main(int argc, const char **argv)
{
  auto options_opt = mr::RenderOptions::parse(argc, argv);
  if (options_opt == std::nullopt) {
    return 1;
  }
  auto options = *options_opt;
  options.print();

  if (options.mode == mr::RenderOptions::Mode::Bench) {
    std::println("Currently the benchmark mode is not implemented");
    return 0;
  }

  mr::Application app(options.mode == mr::RenderOptions::Mode::Default);
  // TODO(dk6): Check max avaible size of all user's monitors using vkfw if we have Default mode,
  //            else use option options.width/height
  auto render_context = app.create_render_context({4096, 2160});

  auto scene = render_context->create_scene();
  scene->create_directional_light(mr::Norm3f(1, 1, -1));
  for (const auto &model_path : options.models) {
    scene->create_model(model_path);
  }
  if (options.camera) {
    // TODO: set scene camera
  }

  if (options.mode == mr::RenderOptions::Mode::Default) {
    auto window = render_context->create_window({options.width, options.height});
    app.start_render_loop(*render_context, scene, window);
  } else if (options.mode == mr::RenderOptions::Mode::Frames) {
    auto file_writer = render_context->create_file_writer({options.width, options.height});
    app.render_frames(*render_context, scene, file_writer,
                      options.dst_dir, "frame", options.frames_number);
  }
}
