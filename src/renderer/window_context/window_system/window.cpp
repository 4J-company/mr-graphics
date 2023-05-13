#include "window.hpp"
#include <thread>

// size-based constructor
// arguments:
//   - size:
//       size_t width, height
window_system::window::window(size_t width, size_t height)
    : _width(width), _height(height) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  _window =
      glfwCreateWindow(width, height, "Vulkan renderer", nullptr, nullptr);

  // create thread responsible for this window
  _thread = std::thread([&]() {
    while (!glfwWindowShouldClose(_window)) {
      glfwPollEvents();
    }
  });
} // end of 'window_system::window::window' function

window_system::window::~window() {
  _thread.join();
  glfwDestroyWindow(_window);
}

window_system::application::application() { glfwInit(); }

window_system::application::~application() { glfwTerminate(); }
