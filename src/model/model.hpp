#ifndef __MR_MODEL_HPP_
#define __MR_MODEL_HPP_

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
      std::vector<mr::MaterialHandle> _materials;

      std::string _name;

      void _process_node(const VulkanState &state,
                         const vk::RenderPass &render_pass,
                         tinygltf::Model &model,
                         const mr::Matr4f &parent_transform,
                         mr::UniformBuffer &cam_ubo,
                         const tinygltf::Node &node) noexcept;

    public:
      /**
 * @brief Default constructor.
 *
 * Constructs an empty Model with default-initialized members.
 */
Model() = default;

      Model(const VulkanState &state, vk::RenderPass render_pass, std::string_view filename, mr::UniformBuffer &cam_ubo) noexcept;

      /**
 * @brief Default copy constructor.
 *
 * Creates a new Model instance by performing a member-wise copy of the provided object. This constructor is defaulted and marked as noexcept.
 *
 * @param other The Model instance to copy.
 */
Model(const Model &other) noexcept = default;
      /**
 * @brief Default copy assignment operator.
 *
 * Performs a member-wise copy of the contents from the given Model instance, leveraging
 * the compiler-generated default behavior. This operation is guaranteed not to throw.
 *
 * @param other The source Model instance to copy.
 * @return A reference to this Model after assignment.
 */
Model &operator=(const Model &other) noexcept = default;

      /**
 * @brief Move constructor for the Model class.
 *
 * Transfers resources from the given Model, leaving it in a valid but unspecified state.
 *
 * @param other The Model instance to move from.
 */
Model(Model &&other) noexcept = default;
      /**
 * @brief Move-assigns another Model to this instance.
 *
 * Transfers ownership of resources from the provided temporary Model to this object.
 *
 * @param other The Model instance to move from.
 * @return Model& Reference to this object.
 */
Model &operator=(Model &&other) noexcept = default;

      void draw(CommandUnit &unit) const noexcept;
  };
} // namespace mr

#endif // __MR_MODEL_HPP_
