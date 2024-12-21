#ifndef __log_hpp_
#define __trace_hpp_

#include "pch.hpp"

namespace mr::detail::term_modifier {
  constexpr const char *identity = "";
  constexpr const char *reset = "\033[0m";
  constexpr const char *red = "\033[31m";
  constexpr const char *yellow = "\033[33m";
  constexpr const char *magenta = "\033[35m";
}

#ifndef NDEBUG
#define MR_LOG(category, modifier, format, ...)\
  do\
  {\
    std::print("{}{}: ", modifier, category);\
    std::println(format, __VA_ARGS__);\
    std::print("{}", mr::detail::term_modifier::reset);\
  } while (false)

#else
#define MR_LOG(category, format, ...) static_cast<void>(0)
#endif // NDEBUG

#define MR_INFO(format, ...) MR_LOG("INFO", mr::detail::term_modifier::identity, format, __VA_ARGS__)
#define MR_DEBUG(format, ...) MR_LOG("DEBUG", mr::detail::term_modifier::magenta, format, __VA_ARGS__)
#define MR_WARNING(format, ...) MR_LOG("WARNING", mr::detail::term_modifier::yellow, format, __VA_ARGS__)
#define MR_ERROR(format, ...) MR_LOG("ERROR", mr::detail::term_modifier::red, format, __VA_ARGS__)
#define MR_FATAL(format, ...) MR_LOG("FATAL", mr::detail::term_modifier::red, format, __VA_ARGS__)

#endif // __trace_hpp_
