#ifndef __model_hpp_
#define __model_hpp_

#define TINYGLTF_NO_INCLUDE_RAPIDJSON
#define TINYGLTF_NOEXCEPTION
#include "tiny_gltf.h"

#include "renderer/mesh/mesh.hpp"
#include "renderer/material/material.hpp"

namespace mr {
  class VulkanState;

  class Model {
    private:
      std::vector<mr::Mesh> _meshes;
      std::vector<mr::Material> _materials;

      std::string _name;

      void _process_node(const VulkanState &state,
                         const vk::RenderPass &render_pass,
                         tinygltf::Model &model,
                         const mr::Matr4f &parent_transform,
                         const tinygltf::Node &node) noexcept;

    public:
      Model() = default;

      Model(const VulkanState &state, vk::RenderPass render_pass, std::string_view filename) noexcept;

      Model(const Model &other) noexcept = default;
      Model &operator=(const Model &other) noexcept = default;

      Model(Model &&other) noexcept = default;
      Model &operator=(Model &&other) noexcept = default;

      void draw(CommandUnit &unit) const noexcept;
  };
} // namespace mr

#endif // __model_hpp_
