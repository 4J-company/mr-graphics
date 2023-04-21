#if !defined(__window_hpp_)
#define __window_hpp_

#include "pch.hpp"

namespace window_system
{
  // window class declaration (handles single window)
  class window : public Gtk::Window 
  {
  private:
    size_t _width = 0, _height = 0;
    Gtk::Button _button;

  public:
    window( size_t width = 640, size_t height = 480 );
  }; // end of 'window' class

  // window system class declaration (includes Gtk::Application instance)
  class application : public Gtk::Application 
  {
    friend class window;

  private:
    inline static application *handle = nullptr;

    void on_activate() override
    {
      // start up signaled 
      // create window
      static window *Win = new window();
    }
  public:
    application( int argc, char **argv ) : Gtk::Application("org.gtkmm.examples.application", Gio::Application::Flags::HANDLES_OPEN)
    {
       if (handle != nullptr)
         std::cout << "can't create second applicaiotn instance" << std::endl;
       handle = this;
       run(argc, argv);
    }
  }; // end of 'window_system' class
}

#endif // __window_hpp_

