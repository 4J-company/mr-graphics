#if !defined(__window_hpp_)
#define __window_hpp_

#include "pch.hpp"

namespace window_system
{
  // forward declaration
  class window;

  // window system class declaration (includes Gtk::Application instance)
  class application 
  {
    friend class window;

  private:
    inline static auto handle = Gtk::Application::create("org.gtkmm.examples.base");
  public:
    application( int argc, char **argv )
    {
      handle->make_window_and_run<window>(argc, argv);
    }
  }; // end of 'window_system' class

  // window class declaration (handles single window)
  class window : public Gtk::Window 
  {
  private:
    size_t _width = 0, _height = 0;
    Gtk::Button _button;

  public:
    window( size_t width = 640, size_t height = 480 );
  }; // end of 'window' class
}

#endif // __window_hpp_

