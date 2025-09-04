#ifndef __MR_SCENE_HPP_
#define __MR_SCENE_HPP_

#include "lights/lights.hpp"
#include "model/model.hpp"
#include "manager/resource.hpp"
#include "renderer/window/input_state.hpp"

namespace mr {
  // forward declaration of render context class
  class RenderContext;

  class Scene : public ResourceBase<Scene> {
    friend class RenderContext;

  private:
    const RenderContext *_parent = nullptr;

    // One array for each light type
    std::tuple<
      std::vector<DirectionalLightHandle>
    > _lights;

    template <std::derived_from<Light> L>
    constexpr std::vector<Handle<L>> & lights() noexcept { return std::get<get_light_type<L>()>(_lights); }

    std::vector<ModelHandle> _models;

    mutable UniformBuffer _camera_uniform_buffer;
    mr::FPSCamera _camera;

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
    }

    Scene(Scene &&) = default;
    Scene & operator=(Scene &&) = default;

    Scene(const RenderContext &render_context);

    const RenderContext & render_context() const noexcept { return *_parent; }
    UniformBuffer & camera_uniform_buffer() const noexcept { return _camera_uniform_buffer; }

    void update(const InputState &input_state) noexcept;
  };

  MR_DECLARE_HANDLE(Scene);
} // end of mr namespace

#endif // __MR_SCENE_HPP_