#ifndef __MR_RENDER_CONTEXT_OPTIONS_HPP_
#define __MR_RENDER_CONTEXT_OPTIONS_HPP_

#include "pch.hpp"

namespace mr {
namespace graphics {
  enum struct RenderOptions : uint32_t {
    None           = 0,
    DisableCulling = (1u << 0),
    EnableVsync    = (1u << 1),
  };

  constexpr static inline RenderOptions operator&(RenderOptions options, RenderOptions option) noexcept
  {
    return enum_cast<RenderOptions>(enum_cast(options) & enum_cast(option));
  }

  constexpr static inline RenderOptions operator|(RenderOptions options, RenderOptions option) noexcept
  {
    return enum_cast<RenderOptions>(enum_cast(options) | enum_cast(option));
  }

  constexpr static inline RenderOptions operator&=(RenderOptions &options, RenderOptions option) noexcept
  {
    return (options = options & option);
  }

  constexpr static inline RenderOptions operator|=(RenderOptions &options, RenderOptions option) noexcept
  {
    return (options = options | option);
  }

  constexpr static inline bool is_render_option_enabled(RenderOptions options, RenderOptions option)
  {
    return (options & option) != RenderOptions::None;
  }
} // end of 'graphics' namespace
} // end of 'mr' namespace

#endif // __MR_RENDER_CONTEXT_OPTIONS_HPP_
