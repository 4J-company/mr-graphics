#ifndef __window_hpp_
#define __window_hpp_

#include "pch.hpp"

namespace ter {
class window_context;
}

namespace window_system {
class application;

class window {
  friend class application;
  friend class ter::window_context;

private:
  size_t _width, _height;
  GLFWwindow *_window = nullptr;

public:
  window(size_t width, size_t height);
  ~window();
};

class application {
public:
  application();
  ~application();
};
} // namespace window_system
#endif
