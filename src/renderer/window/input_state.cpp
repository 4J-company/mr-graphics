#include "input_state.hpp"

mr::InputState::InputState()
{
  _writer_key_pressed = std::span(_key_pressed.data(), _key_pressed.size());
  _reader_key_pressed = std::span(_prev_key_pressed.data(), _prev_key_pressed.size());

  _writer_key_tapped = std::span(_key_tapped.data(), _key_tapped.size());
  _reader_key_tapped = std::span(_prev_key_tapped.data(), _prev_key_tapped.size());
}

void mr::InputState::update() noexcept
{
  Vec2d mouse_pos_copy;
  std::ranges::fill(_reader_key_pressed, 0);
  std::ranges::fill(_reader_key_tapped, 0);

  /* scope for mutex lock */ {
    std::lock_guard lock(_update_mutex);
    std::copy(_writer_key_pressed.begin(), _writer_key_pressed.end(), _reader_key_pressed.begin());
    std::swap(_reader_key_tapped, _writer_key_tapped);
    mouse_pos_copy = _mouse_pos;
  }

  _mouse_pos_delta = mouse_pos_copy - _prev_mouse_pos;
  _prev_mouse_pos = mouse_pos_copy;

  _mouse_in_screen_at_last_frame = _mouse_in_screen.load();

  _prev_mouse_scroll_offset = _mouse_scroll_offset.exchange(0);
}

bool mr::InputState::key_pressed(vkfw::Key key) const noexcept
{
  auto idx = std::to_underlying(key);
  ASSERT(idx < max_keys_number);
  return _reader_key_pressed[idx];
}

bool mr::InputState::key_tapped(vkfw::Key key) const noexcept
{
  auto idx = std::to_underlying(key);
  ASSERT(idx < max_keys_number);
  return _reader_key_tapped[idx];
}

void mr::InputState::on_key(const vkfw::Window &, vkfw::Key key, int,
                            vkfw::KeyAction action, vkfw::ModifierKeyFlags)
{
  std::lock_guard lock(_update_mutex);
  auto idx = std::to_underlying(key);
  if (idx >= _writer_key_tapped.size()) {
    return;
  }
  if (action == vkfw::KeyAction::ePress) {
    _writer_key_tapped[idx] = true;
  }
  _writer_key_pressed[idx] = action != vkfw::KeyAction::eRelease;
}

void mr::InputState::on_mouse_move(const vkfw::Window &, double x, double y)
{
  std::lock_guard lock(_update_mutex);
  _mouse_pos = {x, y};
  if (not _mouse_in_screen_at_last_frame) {
    _prev_mouse_pos = _mouse_pos;
  }
}

void mr::InputState::on_mouse_enter(const vkfw::Window &, bool entered)
{
  _mouse_in_screen = entered;
}

void mr::InputState::on_scroll(const vkfw::Window &, double, double yoff)
{
  _mouse_scroll_offset += yoff;
}

void mr::register_input_state_with_window(InputState &state, vkfw::Window &window)
{
  window.callbacks()->on_cursor_move = [&state](const vkfw::Window &w, double x, double y) {
    state.on_mouse_move(w, x, y);
  };
  window.callbacks()->on_key = [&state](const vkfw::Window &w, vkfw::Key key, int scan_code,
                                         vkfw::KeyAction action, vkfw::ModifierKeyFlags flags) {
    state.on_key(w, key, scan_code, action, flags);
  };
  window.callbacks()->on_scroll = [&state](const vkfw::Window &w, double xoff, double yoff) {
    state.on_scroll(w, xoff, yoff);
  };
  window.callbacks()->on_cursor_enter = [&state](const vkfw::Window &w, bool entered) {
    state.on_mouse_enter(w, entered);
  };
}

mr::InputState::InputState(InputState &&other) noexcept
{
  *this = std::move(other);
}

mr::InputState & mr::InputState::operator=(InputState &&other) noexcept
{
  _key_pressed = std::move(other._key_pressed);
  _prev_key_pressed = std::move(other._prev_key_pressed);
  _key_tapped = std::move(other._key_tapped);
  _prev_key_pressed = std::move(other._prev_key_pressed);

  _writer_key_pressed = std::span(_key_pressed.data(), _key_pressed.size());
  _reader_key_pressed = std::span(_prev_key_pressed.data(), _prev_key_pressed.size());
  _writer_key_tapped = std::span(_key_tapped.data(), _key_tapped.size());
  _reader_key_tapped = std::span(_prev_key_tapped.data(), _prev_key_tapped.size());
  // if reader refers on _key_pressed, writer refers to _prev_key_pressed - in new instance after move must be same state
  if (other._reader_key_pressed.data() == other._key_pressed.data()) {
    std::swap(_writer_key_pressed, _reader_key_pressed);
    std::swap(_writer_key_tapped, _reader_key_tapped);
  }

  _mouse_pos = other._mouse_pos;
  _prev_mouse_pos = other._prev_mouse_pos;
  _mouse_pos_delta = other._mouse_pos_delta;

  _prev_mouse_scroll_offset = other._prev_mouse_scroll_offset.load();

  return *this;
}
