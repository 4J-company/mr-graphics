#ifndef __window_hpp_
#define __window_hpp_

#include "pch.hpp"
#include "window_context.hpp"

namespace mr
{
  class WindowContext;

  class Kernel;

  class Window
  {
  private:
    size_t _width, _height;
    xwin::Window _window;
    xwin::EventQueue _event_queue;
    std::thread _thread;

    mr::WindowContext _context;

  public:
    // constructors
    Window(mr::VulkanState state, size_t width = 800, size_t height = 600);
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

  class Kernel
  {
  public:
    Kernel();
    ~Kernel();
  };
} // namespace mr
#endif // __window_hpp_
