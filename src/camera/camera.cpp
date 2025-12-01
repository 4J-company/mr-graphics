#include "camera.hpp"

std::array<mr::Vec4f, 6> mr::FPSCamera::frustum_planes() const noexcept
{
  auto vp = viewproj().transposed();
  // xyz - normal, w - distance
  std::array<Vec4f, 6> planes {
    vp[3] + vp[0], // Left plane
    vp[3] - vp[0], // Right plane
    vp[3] + vp[1], // Bottom plane
    vp[3] - vp[1], // Top plane
    vp[3] + vp[2], // Near plane
    vp[3] - vp[2], // Far plane
  };
  for (auto &plane : planes) {
    auto norm_len = Vec3f(plane.x(), plane.y(), plane.z()).length();
    plane /= norm_len;
  }
  return planes;
}
