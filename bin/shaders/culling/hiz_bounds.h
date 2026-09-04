#ifndef _HIZ_BOUNDS_H_
#define _HIZ_BOUNDS_H_

#include "bounds.h"

float get_bound_sphere_depth(CameraData camera, BoundSphere bs)
{
  vec3 dir_to_cam = normalize(camera.pos.xyz - bs.center);
  vec4 projected_center = camera.vp * vec4(bs.center + dir_to_cam * bs.radius, 1.0);
  return projected_center.z / projected_center.w;
}

bool project_sphere(CameraData camera, BoundSphere bs, out vec4 rectangle)
{
  vec3 center_in_view = (camera.view * vec4(bs.center, 1)).xyz;
  float p00 = camera.proj[0][0];
  float p11 = camera.proj[1][1];
  return get_bound_sphere_screen_rectangle(center_in_view, bs.radius, camera.near, p00, p11, rectangle);
}

#endif // _HIZ_BOUNDS_H_
