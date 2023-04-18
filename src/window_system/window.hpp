#if !defined(__window_hpp_)
#define __window_hpp_

#include <gtkmm.h>

namespace window_system
{
  class window_system
  {
  private:
    Gtk::Application _app;

  public:
    window_system() = default;
  };

  class window 
  {
  private:
    size_t _width = 0, _height = 0;

  public:
    // default constructor
    window() = default;

    // size-based constructor
    // arguments:
    //   - size:
    //       size_t width, height
    window( size_t width, size_t height );
  };
}

#endif // __window_hpp_

