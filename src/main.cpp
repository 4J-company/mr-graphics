#include <system.hpp>
#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"

int main(int argc, const char **argv)
{
  if (argc != 2) {
    MR_ERROR("Usage: model-renderer /path/to/model.gltf");
    exit(1);
  }

  mr::Application app;

  auto render_context = app.create_render_context({4096, 2160});
  auto scene = render_context->create_scene();
  scene->create_directional_light(mr::Norm3f(1, 1, -1));
  scene->create_model(argv[1]);

  // auto file_writer = render_context->create_file_writer();
  // app.render_frames(*render_context, scene, file_writer, "frame", 1);
  // std::terminate(); // for skip deinitialization validation layer errors

  auto window = render_context->create_window({1920, 1080});
  app.start_render_loop(*render_context, scene, window);
}
