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
#include <map>
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

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

// WinAPI macros undefined :(
#undef max
#undef min

namespace mr {
  using uint = unsigned int;
  using byte = unsigned char;

  template<typename... Ts> struct Overloads : Ts... { using Ts::operator()...; };
} // namespace mr

#endif // __pch_hpp_
