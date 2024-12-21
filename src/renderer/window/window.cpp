#include "window.hpp"
#include "camera/camera.hpp"

// size-based constructor
// arguments:
//   - size:
//       size_t width, height
mr::Window::Window(const VulkanState &state, size_t width, size_t height)
    : _width(width)
    , _height(height)
{
  static mr::FPSCamera camera = state;
  _cam = &camera;

  // TODO: retry a couple of times
  vkfw::WindowHints hints{};
  hints.resizable = true;
  hints.visible = true;
  hints.focusOnShow = true;
  // hints.transparentFramebuffer = true;

  auto [result_code, window] =
    vkfw::createWindowUnique(width, height, "CGSGFOREVER", hints);
  if (result_code != vkfw::Result::eSuccess) {
    exit(1);
  }

  _window = std::move(window);

  glfwSetInputMode(_window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  _window->callbacks()->on_cursor_move = [this](const vkfw::Window &win, double dx, double dy) {
    camera.turn({
        2 * dx / _width - 1,
        2 * dy / _height - 1,
        0});
  };
  _window->callbacks()->on_key = [&](const vkfw::Window &win, vkfw::Key key,
                                    int scan_code, vkfw::KeyAction action,
                                    vkfw::ModifierKeyFlags flags) {
    if (key == vkfw::Key::eEscape) {
      win.setShouldClose(true);
    }

    // camera controls
    if (key == vkfw::Key::eW) {
      camera.move(camera.cam().direction());
    }
    if (key == vkfw::Key::eA) {
      camera.move(-camera.cam().right());
    }
    if (key == vkfw::Key::eS) {
      camera.move(-camera.cam().direction());
    }
    if (key == vkfw::Key::eD) {
      camera.move(camera.cam().right());
    }
    if (key == vkfw::Key::eSpace) {
      camera.move(-camera.cam().up());
    }
    if (key == vkfw::Key::eLeftShift) {
      camera.move(camera.cam().up());
    }

    if (key == vkfw::Key::eF11) {
      if (flags & vkfw::ModifierKeyBits::eShift) {
        win.setSize(640, 480);
      }
      else {
        win.maximize();
      }
    }
  };
  _context = mr::WindowContext(this, state);
}

void mr::Window::start_main_loop() {
  std::jthread render_thread {
    [&](std::stop_token stop_token) {
      while (not stop_token.stop_requested()) { render(); }
    }
  };

  // TMP theme
  while (!_window->shouldClose().value) {
    vkfw::pollEvents();
  }
}
