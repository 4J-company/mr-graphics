#include "window.hpp"

// size-based constructor
// arguments:
//   - size:
//       size_t width, height
window_system::window::window( size_t width, size_t height ) : _width(width), _height(height)
{
  set_title("First window!");
  set_default_size(_width, _height);
  
  application::handle->add_window(*this);

  present();
} // end of 'window_system::window::window' function

