#include "window_context.hpp"
#include <system.hpp>

void xmain(int argc, const char **argv)
{
  ter::Application App;

  auto wnd = std::make_unique<window_system::Window>(800, 600);
  auto wnd_ctx = std::make_unique<ter::WindowContext>(wnd.get(), App);

  // TMP theme
  App.create_render_resourses();
  while (true) App.render();
}
