#ifndef _MSOC_H_
#define _MSOC_H_

#include "bounds.h"
#include "culling/hiz_bounds.h"

#ifndef TILE_SIZE
#define TILE_SIZE 8
#endif // TILE_SIZE

#ifndef TILES_PER_THREAD
#define TILES_PER_THREAD 8
#endif // TILES_PER_THREAD

#ifndef BOUNDS_TYPE_BOXES
#define BOUNDS_TYPE_BOXES 0
#endif

#ifndef BOUNDS_TYPE_SPHERES
#define BOUNDS_TYPE_SPHERES 1
#endif

#ifndef BOUNDS_TYPE_DYNAMIC_BEST
#define BOUNDS_TYPE_DYNAMIC_BEST 2
#endif

#ifndef BOUNDS_TYPE_BOTH
#define BOUNDS_TYPE_BOTH 3
#endif

struct TileDim {
  uint tiles_w;
  uint tiles_h;
};

// Expand AABB to screen-aligned TILE grid (same grid as fixed-mip texels):
//   min -> DIV_DOWN, max -> DIV_UP.
// Without this, tiles start at rect.xy and sample centers miss pyramid texels.
vec4 snap_screen_rect_to_tile_grid(vec4 screen_rect, uint screen_width, uint screen_height)
{
  vec2 tile_uv = vec2(
    float(TILE_SIZE) / float(screen_width),
    float(TILE_SIZE) / float(screen_height)
  );

  vec2 mn = floor(screen_rect.xy / tile_uv) * tile_uv;
  vec2 mx = ceil(screen_rect.zw / tile_uv) * tile_uv;
  // Point / sub-tile rect still covers at least one grid cell.
  mx = max(mx, mn + tile_uv);

  return vec4(mn, mx);
}

uvec2 compute_tile_dims(vec4 screen_rect, uint pyramid_width, uint pyramid_height)
{
  float tile_w_uv = float(TILE_SIZE) / float(pyramid_width);
  float tile_h_uv = float(TILE_SIZE) / float(pyramid_height);

  float rect_w = screen_rect.z - screen_rect.x;
  float rect_h = screen_rect.w - screen_rect.y;

#ifdef MSOC_SNAP_RECT_TO_TILE_GRID
  // screen_rect already snapped; size is k * tile_uv (float-safe round).
  uint tiles_w = max(1u, uint(rect_w / tile_w_uv + 0.5));
  uint tiles_h = max(1u, uint(rect_h / tile_h_uv + 0.5));
#else
  rect_w = max(rect_w, tile_w_uv);
  rect_h = max(rect_h, tile_h_uv);
  uint tiles_w = max(1u, uint(ceil(rect_w / tile_w_uv)));
  uint tiles_h = max(1u, uint(ceil(rect_h / tile_h_uv)));
#endif
  return uvec2(tiles_w, tiles_h);
}

bool is_near_clip_visible(CameraData camera, BoundSphere bs)
{
  return !is_bound_sphere_dont_clips_camera((camera.view * vec4(bs.center, 1)).xyz, bs.radius, camera.near);
}

float get_instance_query_depth(CameraData camera, BoundBox bb, BoundSphere bs)
{
#if (BOUNDS_TYPE == BOUNDS_TYPE_BOXES)
  return get_bound_sphere_depth(camera, bs);
#elif (BOUNDS_TYPE == BOUNDS_TYPE_SPHERES)
  return get_bound_sphere_depth(camera, bs);
#elif (BOUNDS_TYPE == BOUNDS_TYPE_DYNAMIC_BEST)
  return get_bound_sphere_depth(camera, bs);
#elif (BOUNDS_TYPE == BOUNDS_TYPE_BOTH)
  return get_bound_sphere_depth(camera, bs);
#else
  return get_bound_sphere_depth(camera, bs);
#endif
}

#endif // _MSOC_H_
