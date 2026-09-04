#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "types.h"
#include "culling/culling.h"
#include "culling/late_instances_culling/msoc/msoc.h"

#ifndef TILE_MIP_LEVEL
#define TILE_MIP_LEVEL 3
#endif // TILE_MIP_LEVEL

layout(push_constant) uniform PushContants {
  uint instances_number;

  uint depth_pyramid_image_id;
  uint depth_pyramid_mips_scales_buffer_id;
  uint depth_pyramid_width;
  uint depth_pyramid_height;

  uint tile_offset_buffer_id;
  uint tile_dims_buffer_id;
  uint screen_rect_buffer_id;
  uint query_depth_buffer_id;
  uint visible_flag_buffer_id;
} buffers_data;

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer TileOffsetBuffer {
  uint data[];
} TileOffsetBuffers[];
#define tile_offsets TileOffsetBuffers[buffers_data.tile_offset_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer TileDimsBuffer {
  TileDim data[];
} TileDimsBuffers[];
#define tile_dims TileDimsBuffers[buffers_data.tile_dims_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer ScreenRectBuffer {
  vec4 data[];
} ScreenRectBuffers[];
#define screen_rects ScreenRectBuffers[buffers_data.screen_rect_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer QueryDepthBuffer {
  float data[];
} QueryDepthBuffers[];
#define query_depths QueryDepthBuffers[buffers_data.query_depth_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer VisibleFlagBuffer {
  uint data[];
} VisibleFlagBuffers[];
#define visible_flags VisibleFlagBuffers[buffers_data.visible_flag_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DepthMipsScale {
  float scales[32];
} DepthMipScaleUniformBuffers[];
#define depth_mip_scales DepthMipScaleUniformBuffers[buffers_data.depth_pyramid_mips_scales_buffer_id].scales

layout(set = BINDLESS_SET, binding = TEXTURES_BINDING) uniform sampler2D SampledStorageImages[];
#define DepthPyramid SampledStorageImages[buffers_data.depth_pyramid_image_id]

uint find_instance_index(uint global_tile_id, uint instances_number)
{
  uint lo = 0u;
  uint hi = instances_number;
  while (lo < hi) {
    uint mid = (lo + hi) >> 1u;
    if (tile_offsets[mid + 1u] <= global_tile_id) {
      lo = mid + 1u;
    } else if (tile_offsets[mid] > global_tile_id) {
      hi = mid;
    } else {
      return mid;
    }
  }
  return lo;
}

float read_tile_depth(sampler2D depth_pyramid, vec4 screen_rect,
                      uint tile_x, uint tile_y, uvec2 instance_tile_dims,
                      uint pyramid_width, uint pyramid_height)
{
  vec2 tile_size_uv = vec2(
    (screen_rect.z - screen_rect.x) / float(instance_tile_dims.x),
    (screen_rect.w - screen_rect.y) / float(instance_tile_dims.y)
  );

  vec2 tile_center = vec2(screen_rect.x, screen_rect.y) +
    vec2(float(tile_x) + 0.5, float(tile_y) + 0.5) * tile_size_uv;

  vec2 tile_size_px = vec2(
    tile_size_uv.x * float(pyramid_width),
    tile_size_uv.y * float(pyramid_height)
  );
  float level = floor(log2(max(tile_size_px.x, tile_size_px.y)));
  level = clamp(level, 0.0, float(TILE_MIP_LEVEL));

  vec2 mip_scale = vec2(depth_mip_scales[2 * uint(level)], depth_mip_scales[2 * uint(level) + 1]);
  return textureLod(depth_pyramid, tile_center * mip_scale, level).r;
}

bool test_tile(uint instance_id, uint local_tile_id, float query_depth)
{
  TileDim dims = tile_dims[instance_id];
  uint tile_x = local_tile_id % dims.tiles_w;
  uint tile_y = local_tile_id / dims.tiles_w;

  float occluder_depth = read_tile_depth(DepthPyramid, screen_rects[instance_id],
                                         tile_x, tile_y, uvec2(dims.tiles_w, dims.tiles_h),
                                         buffers_data.depth_pyramid_width, buffers_data.depth_pyramid_height);
  return query_depth <= occluder_depth;
}

void main()
{
  uint total_tiles = tile_offsets[buffers_data.instances_number];
  uint worker_id = gl_GlobalInvocationID.x;
  uint g0 = worker_id * uint(TILES_PER_THREAD);
  if (g0 >= total_tiles) {
    return;
  }

  uint instance_id = find_instance_index(g0, buffers_data.instances_number);
  uint prev_instance_id = 0xffffffffu;
  float query_depth = 0.0;

  uint g = g0;
  for (uint n = 0u; n < uint(TILES_PER_THREAD) && g < total_tiles; ++n, ++g) {
#ifdef MSOC_WITH_HIZ_COARSE_CHECK
    while (g >= tile_offsets[instance_id + 1u]) {
      if (instance_id + 2u <= buffers_data.instances_number
          && tile_offsets[instance_id + 1u] == tile_offsets[instance_id + 2u]) {
        instance_id = find_instance_index(g, buffers_data.instances_number);
        break;
      }
      ++instance_id;
    }
#else
    while (g >= tile_offsets[instance_id + 1u]) {
      ++instance_id;
    }
#endif // MSOC_WITH_HIZ_COARSE_CHECK

    if (instance_id != prev_instance_id) {
#ifdef TEST_TILES_EARLY_EXIT
      prev_instance_id = instance_id;
      if (visible_flags[instance_id] != 0u) {
        continue;
      }
      query_depth = query_depths[instance_id];
#else
      query_depth = query_depths[instance_id];
      prev_instance_id = instance_id;
#endif
    }

#ifdef TEST_TILES_EARLY_EXIT
    if (visible_flags[instance_id] != 0u) {
      continue;
    }
#endif

    uint local_tile_id = g - tile_offsets[instance_id];
    if (test_tile(instance_id, local_tile_id, query_depth)) {
      atomicOr(visible_flags[instance_id], 1u);
    }
  }
}
