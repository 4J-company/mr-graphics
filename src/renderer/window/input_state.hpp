#ifndef __MR_INPUT_STATE_HPP_
#define __MR_INPUT_STATE_HPP_

#include "pch.hpp"
#include "vkfw/vkfw.hpp"

namespace mr {
inline namespace graphics {
  // TODO(dk6): now InputState class wasn't tested
  class InputState {
  public:
    constexpr static uint32_t max_keys_number = std::to_underlying(vkfw::Key::eLAST);

    InputState();

    void update() noexcept;

    bool key_pressed(vkfw::Key key) const noexcept;
    bool key_tapped(vkfw::Key key) const noexcept;

    const Vec2d & mouse_pos() const noexcept { return _prev_mouse_pos; }
    const Vec2d & mouse_pos_delta() const noexcept { return _mouse_pos_delta; }
    const double mouse_scroll() const noexcept { return _prev_mouse_scroll_offset; }

    void on_key(const vkfw::Window &window, vkfw::Key key, int scan_code,
                vkfw::KeyAction action, vkfw::ModifierKeyFlags flags);
    void on_mouse_move(const vkfw::Window &window, double x, double y);
    void on_mouse_enter(const vkfw::Window &window, bool entered);
    void on_scroll(const vkfw::Window &window, double xoff, double yoff);

    InputState(InputState &&other) noexcept;
    InputState & operator=(InputState &&other) noexcept;

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

    std::atomic_bool _mouse_in_screen;
    std::atomic_bool _mouse_in_screen_at_last_frame;

    std::atomic<double> _mouse_scroll_offset = 0;
    std::atomic<double> _prev_mouse_scroll_offset = 0;

    // I think mutex here is good - we have one writer, one reader,
    //  but reader works once per frame only for copy ~400 bytes, in other time it have no affect for writer
    // Expected, what update(), key_pressed() and key_tapped() call in one thread, key callback in other
    mutable std::mutex _update_mutex;
  };

  void register_input_state_with_window(InputState &state, vkfw::Window &window);
}
}

#endif // __MR_INPUT_STATE_HPP_
