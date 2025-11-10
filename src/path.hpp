#ifndef MR_PATHS_HPP
#define MR_PATHS_HPP

#include <filesystem>

namespace mr {
  namespace path {
    inline const std::fs::path project_dir = MR_PROJECT_DIR;

    inline const std::fs::path res_dir = MR_RES_DIR;
    inline const std::fs::path models_dir = res_dir / "models";
    inline const std::fs::path shaders_dir = res_dir / "shaders";
    inline const std::fs::path cache_dir = res_dir / "cache";
  }
}

#endif // MR_PATHS_HPP
