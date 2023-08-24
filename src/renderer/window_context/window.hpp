#ifndef __window_hpp_
#define __window_hpp_

#include "pch.hpp"
#include "window_context.hpp"

namespace ter
{
  class WindowContext;
} // namespace ter

namespace wnd
{
  class Application;

  class Window
  {
  private:
    size_t _width, _height;
    xwin::Window _window;
    xwin::EventQueue _event_queue;
    std::thread _thread;

    ter::WindowContext _context;

  public:
    // constructors
    Window(const ter::VulkanState &state, size_t width = 800, size_t height = 600);
    Window(Window &&other) noexcept = default;
    Window &operator=(Window &&other) noexcept = default;

    // destructor
    ~Window();

    // getters
    size_t width() const { return _width; }
    size_t height() const { return _height; }
    xwin::Window *xwindow_ptr() { return &_window; }

    // methods
    void render() { _context.render(); }
  };

  class Application
  {
  public:
    Application();
    ~Application();
  };
} // namespace wnd
#endif // __window_context_hpp_
