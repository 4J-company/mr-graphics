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

  mr::RenderOptions render_options = mr::RenderOptions::None;
  if (options.disable_culling) {
    render_options |= mr::RenderOptions::DisableCulling;
  }
  if (options.enable_vsync) {
    render_options |= mr::RenderOptions::EnableVsync;
  }
  if (options.disable_occlusion_culling) {
    render_options |= mr::RenderOptions::DisableOcclusionCulling;
  }
  if (options.enable_culling_stat) {
    render_options |= mr::RenderOptions::EnableCullingStats;
  }

  auto render_context = app.create_render_context(render_context_extent, render_options);

  if (options.enable_bound_boxes) {
    render_context->enable_bound_boxes();
  } else {
    render_context->disable_bound_boxes();
  }

  auto scene = render_context->create_scene();
  scene->create_directional_light(mr::Norm3f(1, 1, -1));
  scene->create_directional_light(mr::Norm3f(-1, 1, -1));
  scene->create_directional_light(mr::Norm3f(-1, 1, 1));
  scene->create_directional_light(mr::Norm3f(0.3, 1, 0.3));

  auto models = options.models | std::views::transform(
    [&](const auto model_path) {
      return scene->create_model(model_path);
    }) | std::ranges::to<std::vector>();

  uint32_t models_number = options.bench_instances_number.value_or(1'000);
  for (uint32_t i = 0; i < models_number - models.size(); i++) {
    auto rnd = [](float min, float max) -> float {
      float v = float(rand()) / RAND_MAX; // between 0 and 1
      // [0, 1] -> [min, max]:
      return v * (max - min) + min;
    };

    auto model = models[i % models.size()];

    // This for 1000 kittens models
    float max_dist = 20;
    auto scale = rnd(0.7, 2.3);

    auto pos = mr::Vec4f(rnd(-max_dist, max_dist), rnd(-max_dist, max_dist), rnd(-max_dist, max_dist), 0);
    auto scale_vec = mr::Vec4f(scale, scale, scale, 1);
    auto rot_axis_opt = mr::Vec3f(rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)).normalized();
    while (not rot_axis_opt) {
      rot_axis_opt = mr::Vec3f(rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)).normalized();
    }
    auto rot_axis = *rot_axis_opt;
    auto rot_angle = mr::Radiansf(rnd(0, 2 * std::numbers::pi));

    auto translate = (mr::Matr4f::identity() * mr::translate(pos)).transposed();
    auto transform = (translate * mr::scale(scale_vec)) * mr::rotate(rot_axis, rot_angle);

    std::cout << transform << std::endl;
    std::cout << "\n\n";

    scene->add_model_instance(model, transform);
  }

  if (options.camera.has_value()) {
    scene->camera().cam() = options.camera.value();
  }

  auto window = render_context->create_window({options.width, options.height});
  if (options.print_stat) {
    std::ofstream stat_file("stats.json");
    app.start_render_loop(*render_context, scene, window, stat_file);
  } else {
    app.start_render_loop(*render_context, scene, window);
  }
}

