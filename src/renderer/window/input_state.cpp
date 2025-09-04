#include "input_state.hpp"

void mr::InputState::update() noexcept
{
  std::array<bool, max_keys_number> key_pressed_copy;
  Vec2d mouse_pos_copy;
  /* scope for mutex lock */ {
    std::lock_guard lock(update_mutex);
    // std::ranges::copy(std::execution::unseq, _key_pressed, key_pressed_copy.begin());
    std::ranges::copy(_key_pressed, key_pressed_copy.begin());
    mouse_pos_copy = _mouse_pos;
  }

  for (uint32_t i = 0; i < max_keys_number; i++) {
    _key_tapped[i] = key_pressed_copy[i] && !_prev_key_pressed[i];
  }
  std::ranges::copy(key_pressed_copy, _prev_key_pressed.begin());

  _mouse_pos_delta = mouse_pos_copy - _prev_mouse_pos;
  _prev_mouse_pos = mouse_pos_copy;
}

bool mr::InputState::key_pressed(vkfw::Key key) const noexcept
{
  auto idx = std::to_underlying(key);
  ASSERT(idx < max_keys_number);
  return _prev_key_pressed[idx];
}

bool mr::InputState::key_tapped(vkfw::Key key) const noexcept
{
  auto idx = std::to_underlying(key);
  ASSERT(idx < max_keys_number);
  return _key_tapped[idx];
}

mr::InputState::KeyCallbackT mr::InputState::get_key_callback() noexcept
{
  return [this](const vkfw::Window &window, vkfw::Key key, int scan_code,
                vkfw::KeyAction action, vkfw::ModifierKeyFlags flags) {
    std::lock_guard lock(update_mutex);
    _key_pressed[std::to_underlying(key)] = true;
  };
}

mr::InputState::MouseCallbackT mr::InputState::get_mouse_callback() noexcept
{
  return [this](const vkfw::Window &window, double x, double y) {
    std::lock_guard lock(update_mutex);
    _mouse_pos = {x, y};
  };
}