#ifndef __pch_hpp_
#define __pch_hpp_

// libraries includes
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <concepts>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <execution>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <variant>
#include <vector>

// user includes
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_NODISCARD_WARNINGS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#define VKFW_NO_EXCEPTIONS
#define VKFW_NO_NODISCARD_WARNINGS
#define VKFW_NO_STRUCT_CONSTRUCTORS
#include <vkfw/vkfw.hpp>

#include <mr-math/math.hpp>

// WinAPI macros undefined :(
#undef max
#undef min

namespace mr {
  using uint = unsigned int;
  using byte = unsigned char;

  struct Boundbox {
      mr::Vec3f min;
      mr::Vec3f max;
  };

  template<typename... Ts> struct Overloads : Ts... { using Ts::operator()...; };
} // namespace mr

using namespace std::literals;
using namespace mr::literals;

#endif // __pch_hpp_
