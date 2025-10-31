#ifndef __MR_PCH_HPP_
#define __MR_PCH_HPP_

// libraries includes
#include <algorithm>
#include <array>
#include <bitset>
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
#include <print>
#include <queue>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include <boost/unordered_map.hpp>
#include <boost/container/small_vector.hpp>

#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_vector.h>

#define VULKAN_HPP_ASSERT_ON_RESULT
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_NODISCARD_WARNINGS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include <mr-math/math.hpp>
#include <mr-utils/assert.hpp>
#include <mr-utils/misc.hpp>
#include "path.hpp"
#include <mr-utils/log.hpp>
#include <mr-importer/importer.hpp>

#define VKFW_NO_EXCEPTIONS
#define VKFW_NO_NODISCARD_WARNINGS
#define VKFW_NO_STRUCT_CONSTRUCTORS
#include <vkfw/vkfw.hpp>

#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"

// WinAPI macros undefined :(
#undef max
#undef min

namespace std { namespace fs = filesystem; }

namespace mr {
  using uint = unsigned int;
  using byte = unsigned char;

  template <typename T, size_t N = 16>
  using SmallVector = boost::container::small_vector<T, N>;
} // namespace mr

using namespace std::literals;
using namespace mr::literals;

#endif // __MR_PCH_HPP_
