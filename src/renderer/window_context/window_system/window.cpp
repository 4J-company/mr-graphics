#include "window.hpp"

// size-based constructor
// arguments:
//   - size:
//       size_t width, height
window_system::Window::Window(size_t width, size_t height) : _width(width), _height(height)
{
  // 🖼️ Create Window Description
  xwin::WindowDesc windowDesc {
      .width = 1920,
      .height = 1080,
      .visible = true,
      .title = "cgsg forever",
      .name = "cgsg forever",
  };

  assert(_window.create(windowDesc, _event_queue));

  // create thread responsible for this window
  _thread = std::thread([&]() {
    bool isRunning = true;
    while (isRunning)
    {
      // ♻️ Update the event queue
      _event_queue.update();

      // 🎈 Iterate through that queue:
      while (!_event_queue.empty())
      {
        const xwin::Event &event = _event_queue.front();
        if (event.type == xwin::EventType::Close)
          _window.close(), isRunning = false;
        _event_queue.pop();
      }
    }
  });
} // End of 'window_system::window::window' function

window_system::Window::~Window() { _thread.join(); }

window_system::Application::Application() {}

window_system::Application::~Application() {}
