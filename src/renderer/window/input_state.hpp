#ifndef __MR_INPUT_STATE_HPP_
#define __MR_INPUT_STATE_HPP_

#include "pch.hpp"
#include "vkfw/vkfw.hpp"

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

    std::array<bool, max_keys_number> _key_pressed {}, _prev_key_pressed {};
    std::span<bool> _writer_key_pressed {}, _reader_key_pressed {};

    std::array<bool, max_keys_number> _key_tapped {}, _prev_key_tapped {};
    std::span<bool> _writer_key_tapped {}, _reader_key_tapped {};

    // ----------------------
    // Mouse
    // ----------------------

    Vec2d _mouse_pos {};
    Vec2d _prev_mouse_pos {};
    Vec2d _mouse_pos_delta {};

    std::atomic<double> _mouse_scoll_offset = 0;
    std::atomic<double> _prev_mouse_scoll_offset = 0;

    // I think mutex here is good - we have one writer, one reader,
    //  but reader works once per frame only for copy ~400 bytes, in other time it have no affect for writer
    // Expected, what update(), key_pressed() and key_tapped() call in one thread, key callback in other
    mutable std::mutex update_mutex;

  public:
    InputState();

    void update() noexcept;

    bool key_pressed(vkfw::Key key) const noexcept;
    bool key_tapped(vkfw::Key key) const noexcept;

    const Vec2d & mouse_pos() const noexcept { return _prev_mouse_pos; }
    const Vec2d & mouse_pos_delta() const noexcept { return _mouse_pos_delta; }
    const double mouse_scroll() const noexcept { return _prev_mouse_scoll_offset; }

    InputState(InputState &&other) noexcept;
    InputState & operator=(InputState &&other) noexcept;

  private: // for Window only
    friend class Window;

    using KeyCallbackT =
      std::function<void(const vkfw::Window &, vkfw::Key, int, vkfw::KeyAction, vkfw::ModifierKeyFlags)>;
    KeyCallbackT get_key_callback() noexcept;

    using MouseCallbackT = std::function<void(const vkfw::Window &, double, double)>;
    MouseCallbackT get_mouse_callback() noexcept;

    using MouseScrollCallback = std::function<void(const vkfw::Window &, double, double)>;
    MouseScrollCallback get_mouse_scroll_callback() noexcept;
  };
}
}

#endif // __MR_INPUT_STATE_HPP_
