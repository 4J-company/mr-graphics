#ifndef __MR_BASE_LIGHT_HPP_
#define __MR_BASE_LIGHT_HPP_

#include "lights/light_render_data.hpp"

namespace mr {
inline namespace graphics {
  class DirectionalLight;
  // forward declaration of Scene class
  class Scene;

  class Light {
  protected:
    Scene *_scene = nullptr;
    // Light render data can be getted from _scene, it is simple alias
    const LightsRenderData *_lights_render_data = nullptr;

    Vec3f _color {};

    // I think mutable here is nice idea, because shade is const by logic - state of light like direction don't change
    mutable UniformBuffer _uniform_buffer;
    // TODO(dk6): rename it, mt6 was confused
    mutable bool _updated = true;

    std::atomic_bool _enabled = true;

  protected:
    Light(Scene &scene, const Vec3f &color, size_t ubo_size);
    ~Light() = default;

    // Work with light render data fields
    template <typename Self>
    constexpr const ShaderHandle shader(this const Self &self) noexcept
    {
      return self._lights_render_data->shaders[get_light_type<Self>()];
    }

    template <typename Self>
    constexpr const GraphicsPipeline & pipeline(this const Self &self) noexcept
    {
      return self._lights_render_data->pipelines[get_light_type<Self>()];
    }

    constexpr const VertexBuffer & vertex_buffer() const noexcept { return _lights_render_data->screen_vbuf; }
    constexpr const IndexBuffer & index_buffer() const noexcept { return _lights_render_data->screen_ibuf; }
    constexpr const uint32_t screen_index_number() const noexcept { return _lights_render_data->screen_ibuf.element_count(); }
    constexpr const DescriptorSet & lights_descriptor_set() const noexcept { return _lights_render_data->lights_descriptor_set; }

  public:
    const Vec3f & color() const noexcept { return _color; }
    void color(const Vec3f &col) noexcept { _color = col; _updated = true; }

    void shade(CommandUnit &unit) const noexcept { ASSERT(false, "Base light class can noe be shaded"); }

    void enable() noexcept { _enabled = true; }
    void disable() noexcept { _enabled = false; }
    bool enabled() const noexcept { return _enabled; }
  };
}
}

#endif // __MR_BASE_LIGHT_HPP_
