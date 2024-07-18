#include "window.hpp"

mr::Window::Window(VulkanGlobalState *state, Extent extent)
  : _extent(extent)
{
  // TODO: retry a couple of times
  vkfw::WindowHints hints{};
  hints.resizable = true;
  hints.visible = true;
  hints.focusOnShow = true;
  // hints.transparentFramebuffer = true;

  auto [result_code, window] =
    vkfw::createWindowUnique(_extent.width, _extent.height, "CGSGFOREVER", hints);
  if (result_code != vkfw::Result::eSuccess) {
    std::exit(1);
  }

  _window = std::move(window);
  _window->callbacks()->on_key =
    [](const vkfw::Window &win, vkfw::Key key, int scan_code, vkfw::KeyAction action, vkfw::ModifierKeyFlags flags)
    {
      if (key == vkfw::Key::eEscape)
        win.setShouldClose(true);

      if (key == vkfw::Key::eF11) {
        if (flags & vkfw::ModifierKeyBits::eShift)
          win.setSize(640, 480);
        else
          win.maximize();
      }
    };

  _context = RenderContext(state, this);
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
