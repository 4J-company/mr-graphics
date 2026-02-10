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
    friend class Mesh;

  public:
    struct MeshInstances {
      mr::graphics::Mesh mesh;
      uint32_t instances_number;
      std::vector<Matr4f> transforms;

      // TODO(dk6): instead a lot of small buffers for each mesh maybe it is correct to use one big
      //            HeapBuffer and here store only offset
      StorageBuffer intances_render_info_buffer;
      uint32_t intances_render_info_buffer_id = BindlessDescriptorSet::invalid_id;

      // It is mutable because it writes by scene
      mutable uint32_t mesh_scene_id = static_cast<uint32_t>(-1);
      mutable uint32_t mesh_bound_box_id = static_cast<uint32_t>(-1);
    };

  private:
    Scene *_scene = nullptr;

    std::vector<mr::MaterialBuilder> _builders;

    std::vector<MeshInstances> _meshes;
    std::vector<mr::MaterialHandle> _materials;

    std::string _name;

    std::vector<Matr4f> _transforms_data {};
    std::vector<uint32_t> _offsets_of_instances {};

  public:
    Model() = default;

    Model(Scene &scene, std::fs::path filename) noexcept;

    Model(const Model &other) noexcept = default;
    Model &operator=(const Model &other) noexcept = default;

    Model(Model &&other) noexcept = default;
    Model &operator=(Model &&other) noexcept = default;

    // std::span<const mr::graphics::Mesh> meshes() const noexcept { return _meshes; }
    std::span<const mr::graphics::MaterialHandle> materials() const noexcept { return _materials; }
    // First material handle, second mesh reference
    auto draws() const noexcept { return std::views::zip(_materials, _meshes); }

    uint32_t instances_number() const noexcept { return _transforms_data.size(); }
    Matr4f transform(uint32_t instance) const noexcept;
  };

  MR_DECLARE_HANDLE(Model);
}
} // namespace mr

#endif // __MR_MODEL_HPP_
