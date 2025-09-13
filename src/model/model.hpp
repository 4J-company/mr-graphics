#ifndef __MR_MODEL_HPP_
#define __MR_MODEL_HPP_

#include "renderer/mesh/mesh.hpp"
#include "renderer/material/material.hpp"

namespace mr {
inline namespace graphics {
  class VulkanState;
  class Scene;

  class Model : public ResourceBase<Model> {
    friend class Scene;

    private:
      const Scene *_scene = nullptr;

      std::vector<mr::graphics::Mesh> _meshes;
      std::vector<mr::MaterialHandle> _materials;

      std::string _name;

    public:
      Model() = default;

      Model(Scene &scene, std::fs::path filename) noexcept;

      Model(const Model &other) noexcept = default;
      Model &operator=(const Model &other) noexcept = default;

      Model(Model &&other) noexcept = default;
      Model &operator=(Model &&other) noexcept = default;

      void draw(CommandUnit &unit) const noexcept;
  };

  MR_DECLARE_HANDLE(Model);
} // namespace graphics
} // namespace mr

#endif // __MR_MODEL_HPP_
