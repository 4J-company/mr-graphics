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
    struct MeshFillDrawCommandInfo {
      vk::DrawIndexedIndirectCommand draw_command;
      uint32_t bound_boxes_buffer_id;
      uint32_t bound_box_index;
      uint32_t transform_first_index;
      Mesh::RenderInfo render_info;
    };

    // TODO(dk6): destruct all this stuff in Scene destructor
    struct MeshesWithSamePipeline {
      std::vector<const Mesh *> meshes;

      // TODO(dk6): Make them dynamic sizable VectorBuffer
      StorageBuffer commands_buffer; // Draw commands for all rendering meshes
      StorageBuffer draws_commands; // It must have same size as commands_buffer
      StorageBuffer draws_count_buffer; // 4-byte buffer for int
      std::vector<MeshFillDrawCommandInfo> commands_buffer_data;

      uint32_t commands_buffer_id = -1;
      uint32_t draws_commands_buffer_id = -1;
      uint32_t draws_count_buffer_id = -1;

      // It must have same elements as in 'commands_buffer'
      StorageBuffer meshes_render_info; // render data for each mesh
      uint32_t meshes_render_info_id = -1;
    };

  private:
    static inline constexpr int max_scene_instances = 64000;

  private:
    RenderContext *_parent = nullptr;

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

    std::atomic_uint32_t _mesh_offset = 0;

    StorageBuffer _transforms; // transform matrix for each instance
    // Must be same size as _transforms
    StorageBuffer _render_transforms; // transform matrix for each visible instance
    std::vector<mr::Matr4f> _transforms_data;
    uint32_t _transforms_buffer_id;  // id in bindless descriptor set
    uint32_t _render_transforms_buffer_id;  // id in bindless descriptor set

    StorageBuffer _bound_boxes;
    uint32_t _bound_boxes_buffer_id;
    std::vector<AABBf> _bound_boxes_data;

    ConditionalBuffer _visibility; // u32 visibility mask for each draw call
    std::vector<uint32_t> _visibility_data;

    mutable UniformBuffer _camera_uniform_buffer;
    mr::FPSCamera _camera;
    uint32_t _camera_buffer_id;  // id in bindless descriptor set

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
