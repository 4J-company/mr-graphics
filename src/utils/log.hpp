#ifndef __trace_hpp_
#define __trace_hpp_

#include "pch.hpp"

namespace mr::detail::term_modifier {
  constexpr inline const char *identity = "";
  constexpr inline const char *reset = "\033[0m";
  constexpr inline const char *red = "\033[31m";
  constexpr inline const char *yellow = "\033[33m";
  constexpr inline const char *magenta = "\033[35m";
}

#ifndef NDEBUG
#define MR_LOG(category, modifier, ...)\
  do\
  {\
    std::print("{}{}: ", modifier, category);\
    std::println(__VA_ARGS__);\
    std::print("{}", mr::detail::term_modifier::reset);\
  } while (false)

#else
#define MR_LOG(...) static_cast<void>(0)
#endif // NDEBUG

#define MR_INFO(...) MR_LOG("INFO", mr::detail::term_modifier::reset, __VA_ARGS__)
#define MR_DEBUG(...) MR_LOG("DEBUG", mr::detail::term_modifier::magenta, __VA_ARGS__)
#define MR_WARNING(...) MR_LOG("WARNING", mr::detail::term_modifier::yellow, __VA_ARGS__)
#define MR_ERROR(...) MR_LOG("ERROR", mr::detail::term_modifier::red, __VA_ARGS__)
#define MR_FATAL(...) MR_LOG("FATAL", mr::detail::term_modifier::red, __VA_ARGS__)

#endif // __trace_hpp_
