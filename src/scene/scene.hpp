#ifndef __MR_SCENE_HPP_
#define __MR_SCENE_HPP_

#include <filesystem>
#include "lights/lights.hpp"
#include "model/model.hpp"
#include "manager/resource.hpp"
#include "camera/camera.hpp"
#include "renderer/window/input_state.hpp"

namespace mr {
inline namespace graphics {
  // forward declaration of render context class
  class RenderContext;
  class Model;

  class Scene : public ResourceBase<Scene> {
    friend class RenderContext;
    friend class Model;

  private:
    struct MeshInstanceCullingData {
      uint32_t transform_index;
      uint32_t mesh_culling_data_index;
    };

    struct MeshCullingData {
      vk::DrawIndexedIndirectCommand draw_command;
      Mesh::RenderInfo render_info; // just for copy

      uint32_t instance_counter_index; // index of visible instances number counter in counters buffer
      uint32_t bound_box_index;
    };

    // TODO(dk6): destruct all this stuff in Scene destructor
    struct MeshesWithSamePipeline {
      std::vector<const Mesh *> meshes;

      // TODO(dk6): Use dynamic sizable VectorBuffer
      StorageBuffer instances_data_buffer; // Data for each drawed instance
      StorageBuffer meshes_data_buffer; // Draw commands for all rendering meshes
      StorageBuffer draw_commands_buffer; // It must have same size as meshes_data_buffer

      uint32_t draw_counter_index; // index of draws number counter in counters buffer

      std::vector<MeshInstanceCullingData> instances_data_buffer_data;
      std::vector<MeshCullingData> meshes_data_buffer_data;

      uint32_t instances_data_buffer_id = BindlessDescriptorSet::invalid_id;
      uint32_t meshes_data_buffer_id = BindlessDescriptorSet::invalid_id;
      uint32_t draw_commands_buffer_id = BindlessDescriptorSet::invalid_id;

      // It must have same elements as 'meshes_data_buffer'
      StorageBuffer meshes_render_info; // render data for each mesh
      uint32_t meshes_render_info_id = BindlessDescriptorSet::invalid_id;
    };

  private:
    static inline constexpr int max_scene_instances = 64000;

  private:
    RenderContext *_parent = nullptr;

    // For statistic
    std::atomic_uint64_t _vertexes_number;
    std::atomic_uint64_t _triangles_number;

    // One array for each light type
    // TODO(mt6): Maybe use https://github.com/rollbear/columnist or https://github.com/skypjack/entt here
    std::tuple<
      SmallVector<DirectionalLightHandle>
    > _lights;

    SmallVector<ModelHandle> _models;
    boost::unordered_map<GraphicsPipelineHandle, MeshesWithSamePipeline> _draws;

    CommandUnit _transfer_command_unit;
    vk::UniqueSemaphore _transfers_semaphore;
    bool _was_transfer_in_this_frame = false;

    std::atomic_uint32_t _mesh_offset = 0;

    StorageBuffer _transforms; // transform matrix for each instance
    // Must be same size as _transforms
    StorageBuffer _render_transforms; // transform matrix for each visible instance
    std::vector<mr::Matr4f> _transforms_data;
    uint32_t _transforms_buffer_id = BindlessDescriptorSet::invalid_id;  // id in bindless descriptor set
    uint32_t _render_transforms_buffer_id = BindlessDescriptorSet::invalid_id;  // id in bindless descriptor set

    StorageBuffer _bound_boxes;
    uint32_t _bound_boxes_buffer_id = BindlessDescriptorSet::invalid_id;
    std::vector<AABBf> _bound_boxes_data;

    ConditionalBuffer _visibility; // u32 visibility mask for each draw call
    std::vector<uint32_t> _visibility_data;

    // TODO: Use VectorBuffer
    StorageBuffer _counters_buffer; // Buffer for different counters in compute shaders
    uint32_t _counters_buffer_id = BindlessDescriptorSet::invalid_id;
    std::atomic_uint32_t _current_counter_index = 0;

    mutable UniformBuffer _camera_uniform_buffer;
    mr::FPSCamera _camera;
    uint32_t _camera_buffer_id = BindlessDescriptorSet::invalid_id;  // id in bindless descriptor set

    bool _is_buffers_dirty = true;

    template <std::derived_from<Light> L>
    constexpr SmallVector<Handle<L>> & lights() noexcept { return std::get<get_light_type<L>()>(_lights); }

  public:
    // TODO(dk6): maybe change state & light_render_data in light ctr to render_context?
    DirectionalLightHandle create_directional_light(const Norm3f &direction = Norm3f(0, 1, 0),
                                                    const Vec3f &color = Vec3f(1.0)) noexcept;

    ModelHandle create_model(std::fs::path filename) noexcept;

    template <std::derived_from<Light> L>
    void remove(Handle<L> light)
    {
      lights<L>().erase(std::ranges::find(lights<L>(), light));
    }

    void remove(ModelHandle model)
    {
      // TODO(dk6): Also we must delete from _draws array...
      _models.erase(std::ranges::find(_models, model));
      MR_WARNING(
        "mr::Scene::remove(ModelHandle) wastes 84 bytes on CPU."
        "To fix this you would need to recompute offset values for all other models in the scene."
        "Currently we don't have that functionality :("
      );
    }

    Scene(Scene &&) = default;
    Scene & operator=(Scene &&) = default;

    Scene(RenderContext &render_context);
    ~Scene();

    RenderContext & render_context() noexcept { return *_parent; }
    const RenderContext & render_context() const noexcept { return *_parent; }
    UniformBuffer & camera_uniform_buffer() const noexcept { return _camera_uniform_buffer; }

    using OptionalInputStateReference = std::optional<std::reference_wrapper<const InputState>>;
    void update(OptionalInputStateReference input_state = std::nullopt) noexcept;

    bool dirty() const noexcept { return _is_buffers_dirty; }
    void dirty(bool d) noexcept { _is_buffers_dirty = d; }

    uint32_t transforms_buffer_id() const noexcept { return _transforms_buffer_id; }
    uint32_t camera_buffer_id() const noexcept { return _camera_buffer_id; }

    FPSCamera & camera() noexcept { return _camera; }

  private:
    void update_camera_buffer() noexcept;
  };

  MR_DECLARE_HANDLE(Scene);
}
} // end of mr namespace

#endif // __MR_SCENE_HPP_
