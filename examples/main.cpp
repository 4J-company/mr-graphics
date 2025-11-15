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

  mr::Application app(options.mode == mr::RenderOptions::Mode::Default);

  mr::Extent render_context_extent = {};
  if (options.mode == mr::RenderOptions::Mode::Default) {
    auto [vkfw_res, monitors] = vkfw::getMonitors();
    ASSERT(vkfw_res == vkfw::Result::eSuccess);
    uint32_t max_height = 0, max_width = 0;
    for (const auto &monitor : monitors) {
      max_height = std::max(max_height, static_cast<uint32_t>(monitor.getWorkareaHeight().value));
      max_width = std::max(max_width, static_cast<uint32_t>(monitor.getWorkareaWidth().value));
    }
    render_context_extent = {max_width, max_height};
  } else {
    render_context_extent = {options.width, options.height};
  }

  auto render_context = app.create_render_context(render_context_extent);

  if (options.enable_bound_boxes) {
    render_context->enable_bound_boxes();
  } else {
    render_context->disable_bound_boxes();
  }

  auto scene = render_context->create_scene();
  scene->create_directional_light(mr::Norm3f(1, 1, -1));
  for (const auto &model_path : options.models) {
    scene->create_model(model_path);
  }
  if (options.camera) {
    scene->camera().cam() = options.camera.value();
  }

  if (options.mode == mr::RenderOptions::Mode::Default) {
    auto window = render_context->create_window({options.width, options.height});
    app.start_render_loop(*render_context, scene, window);
  } else if (options.mode == mr::RenderOptions::Mode::Frames) {
    auto file_writer = render_context->create_file_writer({options.width, options.height});
    app.render_frames(*render_context, scene, file_writer,
                      options.dst_dir, "frame", options.frames_number);
  } else if (options.mode == mr::RenderOptions::Mode::Bench) {
    std::fs::create_directory(options.stat_dir);
    auto presenter = render_context->create_dummy_presenter({options.width, options.height});
    for (uint32_t i = 0; i < options.frames_number; i++) {
      scene->update();
      render_context->render(scene, *presenter);

      auto &stat = render_context->stat();
      std::ofstream log_file(std::format("{}/frame{}_stat.json", options.stat_dir.c_str(), i));
      stat.write_to_json(log_file);
    }
  }
}

