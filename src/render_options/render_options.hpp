#ifndef __render_options_hpp_
#define __render_options_hpp_

#include "pch.hpp"

namespace mr {
inline namespace graphics {
  struct RenderOptions {
    enum struct Mode {
      Default,
      Frames,
      Bench,
      ModesNumber
    };

    Mode mode;
    uint32_t frames_number;
    std::fs::path dst_dir;
    uint32_t width, height;
    std::optional<mr::math::Camera<float>> camera;
    bool disable_culling;
    std::fs::path stat_dir;
    std::vector<std::fs::path> models;
    std::optional<std::string> bench_name;

    static std::optional<RenderOptions> parse(int argc, const char **argv);

    void print() const noexcept;
  };
} // end of 'mr' namespace
} // end of 'graphics' namespace

#endif // __render_options_hpp_
