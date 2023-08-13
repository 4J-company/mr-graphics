#ifndef __pch_hpp_
#define __pch_hpp_

// libraries includes
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <execution>
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
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <CrossWindow/CrossWindow.h>
#include <CrossWindow/Graphics.h>
#include <vulkan/vulkan.hpp>

#define __DEBUG false

// WinAPI macros undefined
#undef max(a, b)

#endif // __pch_hpp_
