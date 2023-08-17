#include "timer/timer.hpp"

/* Update timer data */

template <std::floating_point T> void ter::Timer<T>::update()
{
  _global_time =
      std::chrono::duration<T>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - _init_time;
  _global_delta_time = _global_time - _old_time;
  if (_pause)
  {
    _delta_time = 0;
    _pause_time += _global_delta_time;
  }
  else
  {
    _time = _global_time - _pause_time - _init_time;
    _delta_time = _global_delta_time;
  }
  _old_time = _global_time;

  _fps = static_cast<T>(1.0) / _global_delta_time;
}
