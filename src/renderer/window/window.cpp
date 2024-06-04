#include "window.hpp"

// size-based constructor
// arguments:
//   - size:
//       size_t width, height
mr::Window::Window(const VulkanState &state, size_t width, size_t height) : _width(width), _height(height)
{
  // TODO: retry a couple of times
  auto [result_code, window] = vkfw::createWindowUnique(640, 480, "CGSGFOREVER");
  if (result_code != vkfw::Result::eSuccess) {
    exit(1);
  }
  _window = std::move(window);

  _thread = std::jthread([&]() {
      while (!_window->shouldClose().value) {
      }
    });

  _context = mr::WindowContext(this, state);
} // End of 'wnd::window::window' function
