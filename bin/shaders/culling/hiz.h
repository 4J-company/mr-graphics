#ifndef _HIZ_H_
#define _HIZ_H_

#include "culling/hiz_bounds.h"

#ifndef BOUNDS_TYPE
#define BOUNDS_TYPE 0
#endif // BOUNDS_TYPE

#ifndef BOUNDS_TYPE_BOXES
#define BOUNDS_TYPE_BOXES 0
#endif // BOUNDS_TYPE_BOXES

#ifndef BOUNDS_TYPE_SPHERES
#define BOUNDS_TYPE_SPHERES 1
#endif // BOUNDS_TYPE_SPHERES

#ifndef BOUNDS_TYPE_DYNAMIC_BEST
#define BOUNDS_TYPE_DYNAMIC_BEST 2
#endif // BOUNDS_TYPE_DYNAMIC_BEST

#ifndef BOUNDS_TYPE_BOTH
#define BOUNDS_TYPE_BOTH 3
#endif // BOUNDS_TYPE_BOTH

float read_hiz_depth(vec4 rectangle)
{
  rectangle = clamp(rectangle, vec4(-1), vec4(1));

  vec2 rectangle_size = rectangle.zw - rectangle.xy;
  // It is very important to exact pick rectangle center - when minmax sampler will fetch all 2x2 quad data
  vec2 rectangle_center = rectangle.xy + (rectangle_size / 2);

  // Find mip level where bound rectangle is covered by 2x2 pixel quad
  float level = floor(log2(max(
    rectangle_size.x * buffers_data.depth_pyramid_width,
    rectangle_size.y * buffers_data.depth_pyramid_heigth
  )));

  vec2 mip_scale = vec2(depth_mip_scales[2 * uint(level)], depth_mip_scales[2 * uint(level) + 1]);
  vec2 tex_coord = rectangle_center * mip_scale;

  // Using max sampler get max depth of 2x2 quad which covers rectangle
  float old_depth = textureLod(DepthPyramid, tex_coord, level).r;

  return old_depth;
}

bool test_hiz_visible(vec4 rectangle, float query_depth)
{
  return query_depth <= read_hiz_depth(rectangle);
}

float get_bound_box_depth(CameraData camera, BoundBox bb)
{
  vec3 bb_center = (bb.min.xyz + bb.max.xyz) / 2;

  vec3 dir_to_cam = normalize(camera.pos.xyz - bb_center);
  float bb_size = length(bb.max.xyz - bb.min.xyz) / 2;
  vec3 closest_bb_point = bb_center + dir_to_cam * bb_size;

  vec4 projected = (camera.vp * vec4(closest_bb_point, 1));
  float new_depth = projected.z / projected.w;

  return new_depth;
}

bool is_visible_bound_box(CameraData camera, BoundBox bb, BoundSphere bs)
{
  if (!is_bound_sphere_dont_clips_camera((camera.view * vec4(bs.center, 1)).xyz,
                                         bs.radius, camera.near)) {
    return true;
  }

  vec4 rectangle = get_bound_box_screen_rectangle(bb, camera.vp);
  float new_depth = get_bound_sphere_depth(camera, bs);
  return test_hiz_visible(rectangle, new_depth);
}

bool is_visible_bound_sphere(CameraData camera, BoundSphere bs)
{
  vec4 rectangle;
  if (!project_sphere(camera, bs, rectangle)) {
    return true;
  }
  float new_depth = get_bound_sphere_depth(camera, bs);
  return test_hiz_visible(rectangle, new_depth);
}

// Pick smaller screen-rect of box/sphere, one HiZ sample (true DynamicBest).
bool is_visible_bound_dynamic_best(CameraData camera, BoundBox bb, BoundSphere bs)
{
  if (!is_bound_sphere_dont_clips_camera((camera.view * vec4(bs.center, 1)).xyz,
                                         bs.radius, camera.near)) {
    return true;
  }

  vec4 rectangle;
  vec4 bs_rect;
  if (!project_sphere(camera, bs, bs_rect)) {
    rectangle = get_bound_box_screen_rectangle(bb, camera.vp);
  } else {
    vec4 bb_rect = get_bound_box_screen_rectangle(bb, camera.vp);
    float bb_rect_size = bounds_rectangle_max_dim(bb_rect);
    float bs_rect_size = bounds_rectangle_max_dim(bs_rect);
    rectangle = bb_rect_size < bs_rect_size ? bb_rect : bs_rect;
  }

  float new_depth = get_bound_sphere_depth(camera, bs);
  return test_hiz_visible(rectangle, new_depth);
}

// Visible only if both box and sphere HiZ tests pass (former DynamicBest).
bool is_visible_bound_both(CameraData camera, BoundBox bb, BoundSphere bs)
{
  return is_visible_bound_box(camera, bb, bs) && is_visible_bound_sphere(camera, bs);
}

bool is_instance_hiz_visible(CameraData camera, BoundBox bb, BoundSphere bs)
{
#if (BOUNDS_TYPE == BOUNDS_TYPE_BOXES)
  return is_visible_bound_box(camera, bb, bs);
#elif (BOUNDS_TYPE == BOUNDS_TYPE_SPHERES)
  return is_visible_bound_sphere(camera, bs);
#elif (BOUNDS_TYPE == BOUNDS_TYPE_DYNAMIC_BEST)
  return is_visible_bound_dynamic_best(camera, bb, bs);
#elif (BOUNDS_TYPE == BOUNDS_TYPE_BOTH)
  return is_visible_bound_both(camera, bb, bs);
#else
  return true;
#endif
}

#endif // _HIZ_H_
