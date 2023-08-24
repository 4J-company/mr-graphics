#include "window.hpp"

// size-based constructor
// arguments:
//   - size:
//       size_t width, height
mr::Window::Window(mr::VulkanState state, size_t width, size_t height) : _width(width), _height(height)
{
  std::atomic<bool> window_create;
  _thread = std::thread([&]() {
    xwin::WindowDesc window_desc {
        .x = 0,
        .y = 0,
        .width = 720,
        .height = 720,
        .backgroundColor = 0x0000000,
        .visible = true,
        // .fullscreen = true,
        .title = "cgsg forever",
        .name = "cgsg forever",
    };

    window_create = _window.create(window_desc, _event_queue);

    bool is_running = true;
    while (is_running)
    {
      _event_queue.update();

      // 🎈 Iterate through that queue:
      while (!_event_queue.empty())
      {
        const xwin::Event &event = _event_queue.front();
        if (event.type == xwin::EventType::Close)
          _window.close(), is_running = false;
        _event_queue.pop();
      }
    }
  });

  while (!window_create)
    (void)0;

  _context = mr::WindowContext(this, state);
} // End of 'wnd::window::window' function

mr::Window::~Window() { _thread.join(); }

mr::Kernel::Kernel() {}

mr::Kernel::~Kernel() {}
