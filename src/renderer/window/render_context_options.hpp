#ifndef __MR_RENDER_CONTEXT_OPTIONS_HPP_
#define __MR_RENDER_CONTEXT_OPTIONS_HPP_

#include "pch.hpp"

namespace mr {
inline namespace graphics {
  enum struct RenderOptions : uint32_t {
    None           =            0,
    DisableCulling =            (1u << 0),
    EnableVsync    =            (1u << 1),
    DisableOcclusionCulling =   (1u << 2),
    EnableCullingStats      =   (1u << 3),
    EnableCullingVisualiztion = (1u << 4),
    CollectPosInstanceId =      (1u << 5),
    HashColoring =              (1u << 6),
    MsocWithHizCoarseCheck =    (1u << 7),
    OcDrawAlways =              (1u << 8),
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

  enum struct RenderBoundsState : uint32_t {
    Disable,
    BoundBoxes,
    BoundBoxRectangles,
    BoundSpheres,
    BoundSphereRectangles,
    DynamicBest,
    DynamicBestRectangles,
    StatesNumber,
  };

  enum struct OcclusionCullingBounds : uint32_t {
    Box,
    Sphere,
    DynamicBest, // smaller screen-rect of box/sphere (one HiZ/MSOC test)
    Both,        // visible only if both box and sphere tests pass
    BoundsNumber,
  };

  enum struct OcclusionCullingType : uint32_t {
    TwoPhaseHiZ,
    MaskedSoftware,
    MsocAdaptiveTileSize,
  };

  constexpr bool uses_msoc_pipeline(OcclusionCullingType oc_type) noexcept
  {
    return oc_type == OcclusionCullingType::MaskedSoftware
        || oc_type == OcclusionCullingType::MsocAdaptiveTileSize;
  }

  struct RenderContextConfig {
    RenderOptions options = RenderOptions::None;
    RenderBoundsState bounds_state = RenderBoundsState::Disable;
    OcclusionCullingBounds oc_bounds = OcclusionCullingBounds::Box;
    OcclusionCullingType oc_type = OcclusionCullingType::TwoPhaseHiZ;
    Extent msoc_tile_size = {8, 8};
    uint32_t msoc_tiles_per_thread = 8;
    bool test_tile_early_exit = true;
    // Temporary workaround while instances buffers sizes are not dynamic
    uint32_t max_instance_number_per_object = 10;
  };
} // end of 'graphics' namespace
} // end of 'mr' namespace

#endif // __MR_RENDER_CONTEXT_OPTIONS_HPP_
