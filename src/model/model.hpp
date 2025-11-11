#ifndef __MR_MODEL_HPP_
#define __MR_MODEL_HPP_

#include "renderer/mesh/mesh.hpp"
#include "renderer/material/material.hpp"

namespace mr {
inline namespace graphics {
  class Scene;
  class VulkanState;

  class Model : public ResourceBase<Model> {
    friend class Scene;

    private:
      Scene *_scene = nullptr;

      std::vector<mr::MaterialBuilder> _builders;

      std::vector<mr::graphics::Mesh> _meshes;
      std::vector<mr::MaterialHandle> _materials;

      std::string _name;

      Matr4f _transform = mr::Matr4f::identity();

    public:
      Model() = default;

      Model(Scene &scene, std::fs::path filename) noexcept;

      Model(const Model &other) noexcept = default;
      Model &operator=(const Model &other) noexcept = default;

      Model(Model &&other) noexcept = default;
      Model &operator=(Model &&other) noexcept = default;

      std::span<const mr::graphics::Mesh> meshes() const noexcept { return _meshes; }
      std::span<const mr::graphics::MaterialHandle> materials() const noexcept { return _materials; }
      // First material handle, second mesh reference
      auto draws() const noexcept { return std::views::zip(_materials, _meshes); }

      void transform(const Matr4f &transform) noexcept;
      const Matr4f & transform() { return _transform; }
  };

  MR_DECLARE_HANDLE(Model);
}
} // namespace mr

#endif // __MR_MODEL_HPP_
