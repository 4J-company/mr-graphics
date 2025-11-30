#ifndef __fps_camera_
#define __fps_camera_

#include "pch.hpp"

namespace mr {
inline namespace graphics {
  struct ShaderCameraData {
    mr::Matr4f vp;
    mr::Vec4f campos;
    float fov;
    float gamma;
    float speed;
    float sens;
    std::array<Vec4f, 6> frustum_planes;
  };

  class FPSCamera {
  public:
    FPSCamera() = default;
    FPSCamera(FPSCamera &&) = default;
    FPSCamera & operator=(FPSCamera &&) = default;

    FPSCamera & turn(mr::Vec3f delta) noexcept {
      delta *= _sensetivity;
      _cam += mr::Yaw(mr::Radiansf(delta.x()));
      _cam += mr::Pitch(mr::Radiansf(delta.y()));
      _cam += mr::Roll(-mr::Radiansf(std::acos(_cam.right() & mr::axis::y)) + mr::pi / 2 + mr::Radiansf(delta.z()));
      return *this;
    }

    FPSCamera & move(mr::Vec3f delta) noexcept {
      _cam += delta * _speed;
      return *this;
    }

    mr::Matr4f viewproj() const noexcept {
      return _cam.perspective() * _cam.frustum();
    }

    // getters
    mr::math::Camera<float> & cam() noexcept { return _cam; }
    constexpr mr::Degreesf fov() const noexcept { return _fov; }
    constexpr float gamma() const noexcept { return _gamma; }
    constexpr float speed() const noexcept { return _speed; }
    constexpr float sensetivity() const noexcept { return _sensetivity; }

    // setters
    constexpr void fov(mr::Degreesf fov) noexcept { _fov = fov; }
    constexpr void gamma(float gamma) noexcept { _gamma = gamma; }
    constexpr void speed(float speed) noexcept { _speed = speed; }
    constexpr void sensetivity(float sens) noexcept { _sensetivity = sens; }

    std::array<Vec4f, 6> frustum_planes() const noexcept;

  private:
    mr::math::Camera<float> _cam;
    mr::Degreesf _fov = 90_deg;
    float _gamma = 2.2;
    float _speed = .01;
    float _sensetivity = 1;
  };
}
}

#endif // __fps_camera_
