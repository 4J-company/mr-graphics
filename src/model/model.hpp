#ifndef __MR_MODEL_HPP_
#define __MR_MODEL_HPP_

#define TINYGLTF_NO_INCLUDE_RAPIDJSON
#define TINYGLTF_NOEXCEPTION
#include "tiny_gltf.h"

#include "renderer/mesh/mesh.hpp"
#include "renderer/material/material.hpp"

namespace mr {
  class Scene;

  class Model : public ResourceBase<Model> {
    private:
      const Scene *_scene = nullptr;

      std::vector<mr::Mesh> _meshes;
      std::vector<mr::MaterialHandle> _materials;

      std::string _name;

      void _process_node(tinygltf::Model &model,
                         const mr::Matr4f &parent_transform,
                         const tinygltf::Node &node) noexcept;

    public:
      Model() = default;

      Model(const Scene &scene, std::string_view filename) noexcept;

      Model(const Model &other) noexcept = default;
      Model &operator=(const Model &other) noexcept = default;

      Model(Model &&other) noexcept = default;
      Model &operator=(Model &&other) noexcept = default;

      void draw(CommandUnit &unit) const noexcept;
  };

  MR_DECLARE_HANDLE(Model);
} // namespace mr

#endif // __MR_MODEL_HPP_
