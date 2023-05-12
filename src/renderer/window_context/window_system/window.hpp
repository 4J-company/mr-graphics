#ifndef __window_hpp_
#define __window_hpp_

#include "pch.hpp"

namespace window_system {
class window {
private:
  size_t _width, _height;

public:
  window(size_t width, size_t height);
};
class application {};
} // namespace window_system
#endif
