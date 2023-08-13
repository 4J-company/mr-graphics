#include "window_context.hpp"
#include <system.hpp>

void xmain(int argc, const char **argv)
{
  ter::Application App;

  auto a = std::make_unique<window_system::Window>(800, 600);
  auto b = std::make_unique<ter::WindowContext>(a.get(), App);
}
