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
    const RenderContext *_parent = nullptr;

    // One array for each light type
    // TODO(mt6): Maybe use https://github.com/rollbear/columnist or https://github.com/skypjack/entt here
    std::tuple<
      std::vector<DirectionalLightHandle>
    > _lights;

    std::vector<ModelHandle> _models;

    StorageBuffer _transforms; // transform matrix    for each instance
    std::vector<glm::mat4x4> _transforms_data;

    StorageBuffer _bounds;     // AABB                for each instance
    std::vector<mr::AABBf> _bounds_data;

    ConditionalBuffer _visibility; // u32 visibility mask for each draw call
    std::vector<uint32_t> _visibility_data;

    mutable UniformBuffer _camera_uniform_buffer;
    mr::FPSCamera _camera;

    template <std::derived_from<Light> L>
    constexpr std::vector<Handle<L>> & lights() noexcept { return std::get<get_light_type<L>()>(_lights); }

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
      _models.erase(std::ranges::find(_models, model));
      MR_WARNING(
        "mr::Scene::remove(ModelHandle) wastes 84 bytes on CPU."
        "To fix this you would need to recompute offset values for all other models in the scene."
        "Currently we don't have that functionality :("
      );
    }

    Scene(Scene &&) = default;
    Scene & operator=(Scene &&) = default;

    Scene(const RenderContext &render_context);

    const RenderContext & render_context() const noexcept { return *_parent; }
    UniformBuffer & camera_uniform_buffer() const noexcept { return _camera_uniform_buffer; }

    void update(const InputState &input_state) noexcept;

  private:
    void _update_camera_buffer() noexcept;
  };

  MR_DECLARE_HANDLE(Scene);
}
} // end of mr namespace

#endif // __MR_SCENE_HPP_
