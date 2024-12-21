#ifndef __misc_hpp_
#define __misc_hpp_

#include "pch.hpp"

namespace mr {

  template<std::integral To = size_t, typename Enum> requires std::is_enum_v<Enum>
  constexpr To enum_cast(Enum value) noexcept { return static_cast<To>(value); }

  template<typename Enum, std::integral From> requires std::is_enum_v<Enum>
  constexpr Enum enum_cast(From value) noexcept { return static_cast<Enum>(value); }

}

#endif // __misc_hpp_
