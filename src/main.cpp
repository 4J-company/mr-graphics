#include "window_context.hpp"
#include "window_system/window.hpp"
#include <system.hpp>

int main(int argc, char *argv[]) {
  ter::application App;

  auto a = std::make_unique<window_system::window>(800, 600);
  auto b = std::make_unique<ter::window_context>(a.get(), App);

  return 0;
}
