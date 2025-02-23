#include "window.hpp"
#include "camera/camera.hpp"
#include "utils/log.hpp"

/**
 * @brief Constructs a Vulkan window and sets up input callbacks for camera control.
 *
 * This constructor creates a Vulkan-based window with the specified dimensions, sets window hints to
 * enable resizability, visibility, and focus behavior, and establishes a static FPS camera for navigation.
 * It uses vkfw::createWindowUnique to generate the window and terminates the application if window creation fails.
 *
 * The constructor also disables the system cursor and registers two callbacks:
 * - A cursor movement callback that calculates the position delta and updates the camera's orientation.
 * - A key input callback supporting camera movement with W, A, S, D, Space (move down), Left Shift (move up),
 *   window closure via Escape, and window resizing/toggling using F11 (with Shift to set a fixed 640x480 size).
 *
 * @param extent The desired window dimensions.
 *
 * @note The Vulkan global state parameter is considered a common service and is omitted from parameter documentation.
 */
mr::Window::Window(VulkanGlobalState *state, Extent extent)
  : _extent(extent)
{
  static mr::FPSCamera camera;
  _cam = &camera;

  // TODO: retry a couple of times
  vkfw::WindowHints hints{};
  hints.resizable = true;
  hints.visible = true;
  hints.focusOnShow = true;
  // hints.transparentFramebuffer = true;

  auto [result_code, window] =
    vkfw::createWindowUnique(_extent.width, _extent.height, "CGSGFOREVER", hints);
  if (result_code != vkfw::Result::eSuccess) {
    MR_FATAL("Cannot create window. vkfw::createWindowUnique failed with: {}", vkfw::to_string(result_code));
    std::exit(1);
  }

  _window = std::move(window);

  glfwSetInputMode(_window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  _window->callbacks()->on_cursor_move =
    [this](const vkfw::Window &win, double x, double y) {
    static mr::Vec2d old_pos;
    mr::Vec2d pos = {x, y};
    mr::Vec2d delta = pos - old_pos;
    old_pos = pos;
    camera.turn({delta.x() / _extent.width, delta.y() / _extent.height, 0});
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
  _context = mr::RenderContext(state, this);
}

/**
 * @brief Starts the main loop for rendering and event handling.
 *
 * Spawns a rendering thread that continuously calls the `render()` function until a stop request is signaled.
 * Simultaneously, the function polls for window events until the window is marked to close.
 */
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
