#ifndef __model_hpp_
#define __model_hpp_

#include "renderer/renderer.hpp"

namespace mr {
  class Model {
    private:
      std::vector<mr::Mesh *> _meshes;
      std::string _name;

    public:
      Model() = default;
      Model(std::string_view filename, const Application &app) noexcept;
      Model(const Model &other) noexcept = default;
      Model &operator=(const Model &other) noexcept = default;
      Model(Model &&other) noexcept = default;
      Model &operator=(Model &&other) noexcept = default;
  };
} // namespace mr

#endif // __model_hpp_
