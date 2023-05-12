#include "window.hpp"

// size-based constructor
// arguments:
//   - size:
//       size_t width, height
window_system::window::window(size_t width, size_t height)
    : _width(width), _height(height) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  _window =
      glfwCreateWindow(width, height, "Vulkan renderer", nullptr, nullptr);
} // end of 'window_system::window::window' function

window_system::window::~window() { glfwDestroyWindow(_window); }

window_system::application::application() {
  glfwInit();
  window *wnd = new window(800, 600);

  while (!glfwWindowShouldClose(wnd->_window)) {
    glfwPollEvents();
  }
}

window_system::application::~application() { glfwTerminate(); }
