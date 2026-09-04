#include <system.hpp>
#include <cmath>

#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"
#include "render_options/render_options.hpp"

int main(int argc, const char **argv)
{
  auto options_opt = mr::CliOptions::parse(argc, argv);
  if (options_opt == std::nullopt) {
    return 1;
  }
  auto options = *options_opt;
  options.print();

  mr::Application app(options.mode == mr::CliOptions::Mode::Default);

  mr::Extent render_context_extent = {};
  if (options.mode == mr::CliOptions::Mode::Default) {
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
  std::println("render context extent: {}x{}", render_context_extent.width, render_context_extent.height);
  render_context_extent = {options.width, options.height};

  mr::RenderContextConfig config;

  if (options.disable_culling) {
    config.options |= mr::RenderOptions::DisableCulling;
  }
  if (options.disable_occlusion_culling) {
    config.options |= mr::RenderOptions::DisableOcclusionCulling;
  }
  if (options.enable_vsync) {
    config.options |= mr::RenderOptions::EnableVsync;
  }
  if (options.enable_culling_stat) {
    config.options |= mr::RenderOptions::EnableCullingStats;
  }
  if (options.enable_culling_visualization) {
    config.options |= mr::RenderOptions::EnableCullingVisualiztion;
  }
  if (options.read_gbuf || options.save_gbuffer_frame.has_value()) {
    config.options |= mr::RenderOptions::CollectPosInstanceId;
  }
  if (options.hash_coloring) {
    config.options |= mr::RenderOptions::HashColoring;
  }
  if (options.msoc_with_hiz_coarse) {
    config.options |= mr::RenderOptions::MsocWithHizCoarseCheck;
  }
  if (options.oc_draw_always) {
    config.options |= mr::RenderOptions::OcDrawAlways;
  }

  if (options.bounds_state.has_value()) {
    config.bounds_state = options.bounds_state.value();
  }
  if (options.oc_bounds.has_value()) {
    config.oc_bounds = options.oc_bounds.value();
  }
  if (options.oc_type.has_value()) {
    config.oc_type = options.oc_type.value();
  }
  config.msoc_tile_size = options.msoc_tile_size;
  config.msoc_tiles_per_thread = options.msoc_tiles_per_thread;

  auto render_context = app.create_render_context(render_context_extent, config);

  if (options.enable_bound_boxes) {
    render_context->render_bounds_state(mr::RenderBoundsState::BoundBoxes);
  }

  auto scene = render_context->create_scene();
  scene->create_directional_light(mr::Norm3f(1, 1, -1));
  if (!options.hash_coloring) {
    scene->create_directional_light(mr::Norm3f(-1, 1, -1));
    scene->create_directional_light(mr::Norm3f(-1, 1, 1));
    scene->create_directional_light(mr::Norm3f(0.3, 1, 0.3));
    scene->create_directional_light(mr::Norm3f(-1, -1, -1));
  }

  for (const auto &model_path : options.models) {
    scene->create_model(model_path);
  }

  if (options.camera.has_value()) {
    scene->camera().cam().set(options.camera->position(), options.camera->direction(), options.camera->up());
  }
  if (options.projection.has_value()) {
    scene->camera().cam().projection() = *options.projection;
  }

  if (options.mode == mr::CliOptions::Mode::Default) {
    auto window = render_context->create_window({options.width, options.height});
    if (options.print_stat) {
      std::ofstream stat_file("stats.json");
      app.start_render_loop(*render_context, scene, window, stat_file, options.frames_number,
                            options.save_gbuffer_frame, options.save_gbuffer_image_path);
    } else {
      app.start_render_loop(*render_context, scene, window, std::nullopt, options.frames_number,
                            options.save_gbuffer_frame, options.save_gbuffer_image_path);
    }
  } else if (options.mode == mr::CliOptions::Mode::Frames) {
    auto file_writer = render_context->create_file_writer({options.width, options.height});
    app.render_frames(*render_context, scene, file_writer,
                      options.dst_dir, "frame", options.frames_number);
  } else if (options.mode == mr::CliOptions::Mode::Bench) {
    std::fs::create_directory(options.stat_dir);
    std::ofstream stat_file("stats.json"); // For bench stats are always enabled
    auto presenter = render_context->create_dummy_presenter({options.width, options.height});
    for (uint32_t i = 0; options.frames_number.has_value() ? (i < options.frames_number) : true; i++) {
      scene->update();
      render_context->render(scene, *presenter);
      render_context->prev_stat().write_to_json(stat_file);
    }
  }
}

