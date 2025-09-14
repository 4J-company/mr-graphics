#ifndef __MR_TIMER_HPP_
#define __MR_TIMER_HPP_

#include "pch.hpp"

namespace mr {
inline namespace graphics {
  template <std::floating_point T = float> class Timer {
    public:
      using TimeT = std::chrono::duration<T>;
      using DeltaT = std::chrono::duration<T>;

      void update() noexcept
      {
        _global_time =
          std::chrono::duration_cast<TimeT>(
            std::chrono::high_resolution_clock::now().time_since_epoch()) -
          _init_time;
        _global_delta_time = _global_time - _old_time;
        if (_pause) {
          _delta_time = DeltaT {0};
          _pause_time += std::chrono::duration_cast<TimeT>(_global_delta_time);
        }
        else {
          _time = _global_time - _pause_time;
          _delta_time = _global_delta_time;
        }
        _old_time = _global_time;

        _fps = 1.0 / (_global_delta_time.count() * _delta_to_sec);
      }

      void pause(bool pause) noexcept { _pause = pause; }

      [[nodiscard]] bool pause() const noexcept { return _pause; }

      TimeT time() const noexcept { return _time; }

      DeltaT delta_time() const noexcept { return _delta_time; }

      TimeT global_time() const noexcept { return _global_time; }

      DeltaT global_delta_time() const noexcept { return _global_delta_time; }

      double fps() const noexcept { return _fps; }

    private:
      TimeT _time {};
      TimeT _global_time {};
      TimeT _init_time {std::chrono::duration_cast<TimeT>(
        std::chrono::high_resolution_clock::now().time_since_epoch())};
      TimeT _pause_time {};
      TimeT _old_time = _init_time;

      DeltaT _delta_time {};
      DeltaT _global_delta_time {};

      T _fps {};
      bool _pause {false};

      static constexpr double _delta_to_sec {1.0 / DeltaT::period::den};
  };
}
} // namespace mr

#endif // __MR_TIMER_HPP_
