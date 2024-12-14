#ifndef __paths_hpp_
#define __paths_hpp_

#include <filesystem>

namespace mr {
  namespace path {
    inline const std::filesystem::path project_dir = MR_PROJECT_DIR;

    inline const std::filesystem::path res_dir = MR_RES_DIR;
    inline const std::filesystem::path models_dir = res_dir / "models";
    inline const std::filesystem::path shaders_dir = res_dir / "shaders";
  }
}

#endif // __paths_hpp_
