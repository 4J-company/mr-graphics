#include "input_state.hpp"

void mr::InputState::update() noexcept
{
  for (uint32_t i = 0; i < max_keys_number; i++) {
    _key_tapped[i] = _key_pressed[i] && !_prev_key_pressed[i];
  }
  std::ranges::copy(_key_pressed, _prev_key_pressed.begin());
}

bool mr::InputState::key_pressed(vkfw::Key key) const noexcept
{
  auto idx = std::to_underlying(key);
  ASSERT(idx < max_keys_number);
  return _key_pressed[idx];
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
    // TODO(dk6): potential data race, maybe make it std::array<atomic_bool>
    _key_pressed[std::to_underlying(key)] = true;
  };
}

