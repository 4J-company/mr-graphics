#if !defined(__timer_hpp_)
  #define __timer_hpp_

namespace ter
{
  /* Timer measures time in seconds */
  template <std::floating_point T> class Timer
  {
  public:
    Timer() = default;
    ~Timer() = default;

    Timer(Timer &&) noexcept = default;
    Timer &operator=(Timer &&) noexcept = default;

    /* Update timer data */
    void update();

    /* Set pause flag */
    void set_pause(bool Pause) { _pause = Pause; }

    /* Get pause flag */
    bool get_pause() const { return _pause; }

    /* Get time from timer creation exclude puases (sec) */
    T get_time() const { return _time; }

    /* Get time from previous update exclude pauses (sec) */
    T get_delta_time() const { return _delta_time; }

    /* Get time from timer creation include puases (sec) */
    T get_global_time() const { return _global_time; }

    /* Get time from previous update include pauses (sec) */
    T get_global_delta_time() const { return _global_delta_time; }

    /* Get frames per second count */
    T get_fps() { return _fps; }

  private:
    T _time {},                // Time from timer creation exclude puases (sec)
        _delta_time {},        // Time from previous update exclude pauses (sec)
        _global_time {},       // Time from timer creation include puases (sec)
        _global_delta_time {}, // Time from previous update include pauses (sec)
        _init_time {std::chrono::duration<T>(std::chrono::high_resolution_clock::now().time_since_epoch())
                        .count()}, // Time on timer creation (sec)
        _pause_time {},            // All pauses duration (sec)
        _old_time {_init_time};    // Previous update _global_time (sec)

    T _fps {};           // Frames per second
    bool _pause {false}; // Pause flag
  };                     // end of class 'timer'
} // namespace ter

#endif // __timer_hpp_
