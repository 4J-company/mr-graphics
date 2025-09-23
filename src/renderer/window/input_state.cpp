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
    std::lock_guard lock(update_mutex);
    std::swap(_reader_key_pressed, _writer_key_pressed);
    std::swap(_reader_key_tapped, _writer_key_tapped);
    mouse_pos_copy = _mouse_pos;
  }

  _mouse_pos_delta = mouse_pos_copy - _prev_mouse_pos;
  _prev_mouse_pos = mouse_pos_copy;
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

mr::InputState::KeyCallbackT mr::InputState::get_key_callback() noexcept
{
  return [this](const vkfw::Window &window, vkfw::Key key, int scan_code,
                vkfw::KeyAction action, vkfw::ModifierKeyFlags flags) {
    std::lock_guard lock(update_mutex);
    if (action == vkfw::KeyAction::ePress) {
      _writer_key_tapped[std::to_underlying(key)] = true;
    }
    _writer_key_pressed[std::to_underlying(key)] = true;
  };
}

mr::InputState::MouseCallbackT mr::InputState::get_mouse_callback() noexcept
{
  return [this](const vkfw::Window &window, double x, double y) {
    std::lock_guard lock(update_mutex);
    _mouse_pos = {x, y};
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

  return *this;
}
