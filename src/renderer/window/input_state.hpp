#ifndef __MR_INPUT_STATE_HPP_
#define __MR_INPUT_STATE_HPP_

#include "pch.hpp"
#include "vkfw/vkfw.hpp"

namespace mr {
  // forward declaration of Window class
  class Window;

  class InputState {
    friend class Window;

  public:
    constexpr static uint32_t max_keys_number = std::to_underlying(vkfw::Key::eLAST);
  private:
    // i hope it is not like bool vector
    std::array<bool, max_keys_number> _key_pressed {};
    std::array<bool, max_keys_number> _prev_key_pressed {};
    std::array<bool, max_keys_number> _key_tapped {};

    // TODO(dk6): mouse

  public:
    InputState() = default;

    void update() noexcept;

    bool key_pressed(vkfw::Key key) const noexcept;
    bool key_tapped(vkfw::Key key) const noexcept;

  private: // for Window only
    using KeyCallbackT =
      std::function<void(const vkfw::Window &, vkfw::Key, int, vkfw::KeyAction, vkfw::ModifierKeyFlags)>;
    KeyCallbackT get_key_callback() noexcept;
  };
}

#endif // __MR_INPUT_STATE_HPP_