#ifndef __MR_INPUT_STATE_HPP_
#define __MR_INPUT_STATE_HPP_

#include "pch.hpp"

namespace mr {
inline namespace graphics {
  // forward declaration of Window class
  class Window;

  // TODO(dk6): now InputState class wasn't tested
  class InputState {
  public:
    constexpr static uint32_t max_keys_number = std::to_underlying(vkfw::Key::eLAST);
  private:
    // ----------------------
    // Keyboard
    // ----------------------

    std::array<bool, max_keys_number> _key_pressed {};
    std::array<bool, max_keys_number> _prev_key_pressed {};
    std::array<bool, max_keys_number> _key_tapped {};

    // ----------------------
    // Mouse
    // ----------------------

    Vec2d _mouse_pos {};
    Vec2d _prev_mouse_pos {};
    Vec2d _mouse_pos_delta {};

    // I think mutex here is good - we have one writer, one reader,
    //  but reader works once per frame only for copy ~400 bytes, in other time it have no affect for writer
    // Expected, what update(), key_pressed() and key_tapped() call in one thread, key callback in other
    mutable std::mutex update_mutex;

  public:
    InputState() = default;
    InputState(InputState&& other) noexcept
      : _key_pressed(std::move(other._key_pressed))
      , _prev_key_pressed(std::move(other._prev_key_pressed))
      , _key_tapped(std::move(other._key_tapped))
      , _mouse_pos(std::move(other._mouse_pos))
      , _prev_mouse_pos(std::move(other._prev_mouse_pos))
      , _mouse_pos_delta(std::move(other._mouse_pos_delta))
    {
    }
    InputState& operator=(InputState&& other) noexcept {
      _key_pressed = std::move(other._key_pressed);
      _prev_key_pressed = std::move(other._prev_key_pressed);
      _key_tapped = std::move(other._key_tapped);
      _mouse_pos = std::move(other._mouse_pos);
      _prev_mouse_pos = std::move(other._prev_mouse_pos);
      _mouse_pos_delta = std::move(other._mouse_pos_delta);
    }

    void update() noexcept;

    bool key_pressed(vkfw::Key key) const noexcept;
    bool key_tapped(vkfw::Key key) const noexcept;

    const Vec2d & mouse_pos() const noexcept { return _prev_mouse_pos; }
    const Vec2d & mouse_pos_delta() const noexcept { return _mouse_pos_delta; }

  private: // for Window only
    friend class Window;

    using KeyCallbackT =
      std::function<void(const vkfw::Window &, vkfw::Key, int, vkfw::KeyAction, vkfw::ModifierKeyFlags)>;
    KeyCallbackT get_key_callback() noexcept;

    using MouseCallbackT = std::function<void(const vkfw::Window &, double, double)>;
    MouseCallbackT get_mouse_callback() noexcept;
  };
}
}

#endif // __MR_INPUT_STATE_HPP_
