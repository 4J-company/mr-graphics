// TODO(dk6): include this file to pch.hpp
#ifndef __MR_DIRECTIONAL_LIGHT_HPP_
#define __MR_DIRECTIONAL_LIGHT_HPP_

#include "lights/base/base_light.hpp"
#include "lights/light_render_data.hpp"

namespace mr {
inline namespace graphics {
  class DirectionalLight : public Light, public ResourceBase<DirectionalLight> {
  public:
    struct ShaderUniformBuffer {
      Vec4f direction;
      Vec4f color;
    };

  private:
    Norm3f _direction = Norm3f(1, 1, 1);

    uint32_t _uniform_buffer_id = -1;

  public:
    DirectionalLight(Scene &scene,
                     const Norm3f &direction = Norm3f(1, 1, 1), const Vec3f &color = Vec3f(1.0));
    ~DirectionalLight() noexcept;

    DirectionalLight & operator=(DirectionalLight &&) noexcept = default;
    DirectionalLight(DirectionalLight &&) noexcept = default;

    void shade(CommandUnit &unit) const noexcept;

    const Norm3f & direction() const noexcept { return _direction; }
    void direction(const Norm3f &dir) noexcept { _direction = dir; _updated = true; }

  private:
    void _update_ubo() const noexcept;
  };

  MR_DECLARE_HANDLE(DirectionalLight);
}
}

#endif // __MR_DIRECTIONAL_LIGHT_HPP_
