#ifndef __model_hpp_
#define __model_hpp_

#include "renderer/mesh/mesh.hpp"

namespace mr {
  class VulkanState;

  class Model {
    private:
      std::vector<mr::Mesh> _meshes;
      std::vector<mr::Texture> _textures;
      std::string _name;

    public:
      Model() = default;

      Model(const VulkanState &state, std::string_view filename) noexcept;

      Model(const Model &other) noexcept = default;
      Model &operator=(const Model &other) noexcept = default;

      Model(Model &&other) noexcept = default;
      Model &operator=(Model &&other) noexcept = default;

      void draw(CommandUnit &unit) const noexcept;
  };
} // namespace mr

#endif // __model_hpp_
