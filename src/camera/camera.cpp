#include "camera.hpp"

std::array<mr::Vec4f, 6> mr::FPSCamera::frustum_planes() const noexcept
{
  std::array<Vec4f, 6> planes {};
  // convert to glm matrix
  auto m = viewproj().transposed();

  // xyz - normal
  // w - distance
  
  // Left plane
  planes[0].x(m[0][3] + m[0][0]);
  planes[0].y(m[1][3] + m[1][0]);
  planes[0].z(m[2][3] + m[2][0]);
  planes[0].w(m[3][3] + m[3][0]);
  
  // Right plane
  planes[1].x(m[0][3] - m[0][0]);
  planes[1].y(m[1][3] - m[1][0]);
  planes[1].z(m[2][3] - m[2][0]);
  planes[1].w(m[3][3] - m[3][0]);
  
  // Bottom plane
  planes[2].x(m[0][3] + m[0][1]);
  planes[2].y(m[1][3] + m[1][1]);
  planes[2].z(m[2][3] + m[2][1]);
  planes[2].w(m[3][3] + m[3][1]);

  // Top plane
  planes[3].x(m[0][3] - m[0][1]);
  planes[3].y(m[1][3] - m[1][1]);
  planes[3].z(m[2][3] - m[2][1]);
  planes[3].w(m[3][3] - m[3][1]);
  
  // Near plane
  planes[4].y(m[1][3] + m[1][2]);
  planes[4].x(m[0][3] + m[0][2]);
  planes[4].z(m[2][3] + m[2][2]);
  planes[4].w(m[3][3] + m[3][2]);
  
  // Far plane
  planes[5].x(m[0][3] - m[0][2]);
  planes[5].y(m[1][3] - m[1][2]);
  planes[5].z(m[2][3] - m[2][2]);
  planes[5].w(m[3][3] - m[3][2]);

  for (auto &plane : planes) {
    auto norm_len = Vec3f(plane.x(), plane.y(), plane.z()).length();
    plane /= norm_len;
  }
  return planes;
}

