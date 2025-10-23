#ifndef __MR_SCENE_HPP_
#define __MR_SCENE_HPP_

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
    struct MeshesWithSamePipeline {
      std::vector<const Mesh *> meshes;
      DrawIndirectBuffer commands_buffer;

      // Instead have meshes data and descriptor set for each Pipeline we can use one set per Scene and one big
      // buffer with render data and passed offset to meshes_render_info by push constants.
      // But if so it will be difficult to update scene - for insert data we must move data in memory.
      StorageBuffer meshes_render_info; // render data for each mesh
      std::vector<Mesh::RenderInfo> meshes_render_info_data;
      // It can be replaced on 'meshes_info_id' - register buffer with infos in bindless set and pass it to shader
      // using push constants. But now i don't now which variant will be better.
      DescriptorSet descriptor_set;
    };

  private:
    static inline constexpr int max_scene_instances = 64000;

  private:
    RenderContext *_parent = nullptr;

    // One array for each light type
    // TODO(mt6): Maybe use https://github.com/rollbear/columnist or https://github.com/skypjack/entt here
    std::tuple<
      SmallVector<DirectionalLightHandle>
    > _lights;

    SmallVector<ModelHandle> _models;
    // TODO(dk6): Move of Material change pointer, but now we use materials only from Manager so it is not a problem
    // TODO(dk6): Maybe changes key type from Material to GraphicsPipelines - but now between these class 1:1 relation
    std::unordered_map<const GraphicsPipeline *, MeshesWithSamePipeline> _draws;

    StorageBuffer _transforms; // transform matrix    for each instance
    std::vector<mr::Matr4f> _transforms_data;
    uint32_t _transforms_buffer_id;  // id in bindless descriptor set

    StorageBuffer _bounds;     // AABB                for each instance
    std::vector<mr::AABBf> _bounds_data;

    ConditionalBuffer _visibility; // u32 visibility mask for each draw call
    std::vector<uint32_t> _visibility_data;

    DescriptorSetLayoutHandle _scene_descriptor_set_layout;

    mutable UniformBuffer _camera_uniform_buffer;
    mr::FPSCamera _camera;
    uint32_t _camera_buffer_id;  // id in bindless descriptor set

    template <std::derived_from<Light> L>
    constexpr SmallVector<Handle<L>> & lights() noexcept { return std::get<get_light_type<L>()>(_lights); }

  public:
    // TODO(dk6): maybe change state & light_render_data in light ctr to render_context?
    DirectionalLightHandle create_directional_light(const Norm3f &direction = Norm3f(0, 1, 0),
                                                    const Vec3f &color = Vec3f(1.0)) noexcept;

    ModelHandle create_model(std::string_view filename) noexcept;

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

    uint32_t transforms_buffer_id() const noexcept { return _transforms_buffer_id; }
    uint32_t camera_buffer_id() const noexcept { return _camera_buffer_id; }

    DescriptorSetLayoutHandle scene_set_layout() const noexcept { return _scene_descriptor_set_layout; }

  private:
    void update_camera_buffer() noexcept;
  };

  MR_DECLARE_HANDLE(Scene);
}
} // end of mr namespace

#endif // __MR_SCENE_HPP_
