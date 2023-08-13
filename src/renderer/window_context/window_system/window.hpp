#ifndef __window_hpp_
#define __window_hpp_

#include "pch.hpp"

namespace ter
{
  class WindowContext;
} // namespace ter

namespace window_system
{
  class Application;

  class Window
  {
    friend class Application;
    friend class ter::WindowContext;

  private:
    size_t _width, _height;
    xwin::Window _window;
    xwin::EventQueue _event_queue;
    std::thread _thread;

  public:
    Window(size_t width, size_t height);

    xwin::Window *get_xwindow() { return &_window; }

    ~Window();
  };

  class Application
  {
  public:
    Application();
    ~Application();
  };
} // namespace window_system
#endif // __window_context_hpp_
