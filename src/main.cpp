#include <system.hpp>
#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"

int main(int argc, const char **argv)
{
  mr::Application app;

  auto render_context = app.create_render_context({1920, 1080});
  auto scene = render_context->create_scene();
  scene->create_directional_light(mr::Norm3f(1, 1, -1));
  // scene->create_directional_light(mr::Norm3f(-1, 1, 1));
  scene->create_model("ABeautifulGame/ABeautifulGame.gltf");

  // auto file_writer = render_context->create_file_writer();
  // app.render_frames(*render_context, scene, file_writer, "frame", 10);

  auto window = render_context->create_window();
  app.start_render_loop(*render_context, scene, window);
}
