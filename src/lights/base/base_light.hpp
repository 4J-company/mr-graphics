#ifndef __MR_BASE_LIGHT_HPP_
#define __MR_BASE_LIGHT_HPP_

#include "lights/light_render_data.hpp"

namespace mr {
inline namespace graphics {
  class DirectionalLight;

  class Light {
  protected:
    LightsRenderData *_light_render_data {};

    Vec3f _color {};

    DescriptorSet _set1 {};

    // I think mutable here is nice idea, because shade is const by logic - state of light like direction don't change
    mutable UniformBuffer _uniform_buffer {};
    mutable bool _updated = true;

  protected:
    Light(const VulkanState &state, LightsRenderData &light_render_data, const Vec3f &color, size_t ubo_size);
    ~Light() = default;

    // Work with light render data fields
    template <typename Self>
    constexpr DescriptorAllocator & descriptor_allocator(this const Self &self) noexcept { return self._light_render_data->set1_descriptor_allocators[get_light_type<Self>()]; }

    template <typename Self>
    constexpr DescriptorSetLayoutHandle set_layout(this const Self &self) noexcept { return self._light_render_data->set1_layouts[get_light_type<Self>()]; }

    template <typename Self>
    constexpr ShaderHandle shader(this const Self &self) noexcept { return self._light_render_data->shaders[get_light_type<Self>()]; }

    template <typename Self>
    constexpr GraphicsPipeline & pipeline(this const Self &self) noexcept { return self._light_render_data->pipelines[get_light_type<Self>()]; }

    constexpr VertexBuffer & vertex_buffer() const { return _light_render_data->screen_vbuf; }
    constexpr IndexBuffer & index_buffer() const { return _light_render_data->screen_ibuf; }
    constexpr uint32_t screen_index_number() const { return _light_render_data->screen_ibuf.element_count(); }
    constexpr DescriptorSet & set0() const { return _light_render_data->set0_set; }

  public:
    const Vec3f & color() const noexcept { return _color; }
    void color(const Vec3f &col) noexcept { _color = col; _updated = true; }
  };
}
}

#endif // __MR_BASE_LIGHT_HPP_
