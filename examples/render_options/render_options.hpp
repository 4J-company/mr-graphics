#ifndef __render_options_hpp_
#define __render_options_hpp_

#include <pch.hpp>
#include "renderer/window/render_context_options.hpp"

namespace mr {
inline namespace graphics {
  struct CliOptions {
    enum struct Mode {
      Default,
      Frames,
      Bench,
      ModesNumber
    };

    Mode mode;
    std::optional<uint32_t> frames_number;
    std::fs::path dst_dir;
    uint32_t width, height;
    std::optional<mr::math::Camera<float>> camera;
    std::optional<mr::math::Camera<float>::Projection> projection;
    bool disable_culling;
    bool disable_occlusion_culling;
    bool enable_vsync;
    std::fs::path stat_dir;
    std::vector<std::fs::path> models;
    std::optional<std::string> bench_name;
    std::optional<uint32_t> bench_instances_number;
    bool enable_bound_boxes;
    bool print_stat;
    bool enable_culling_stat;
    bool enable_culling_visualization;
    bool read_gbuf;
    bool hash_coloring;
    std::optional<RenderBoundsState> bounds_state;
    std::optional<OcclusionCullingBounds> oc_bounds;

    static std::optional<CliOptions> parse(int argc, const char **argv);

    void print() const noexcept;
  };
} // end of 'mr' namespace
} // end of 'graphics' namespace

#endif // __render_options_hpp_

