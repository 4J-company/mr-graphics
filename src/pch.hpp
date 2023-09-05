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
#include <string>
#include <thread>
#include <vector>

// user includes
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_NODISCARD_WARNINGS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <CrossWindow/CrossWindow.h>
#include <CrossWindow/Graphics.h>
#include <vulkan/vulkan.hpp>

// WinAPI macros undefined :(
#undef max(a, b)

namespace mr
{
  using uint = unsigned int;
  using byte = unsigned char;
} // namespace mr

#endif // __pch_hpp_
