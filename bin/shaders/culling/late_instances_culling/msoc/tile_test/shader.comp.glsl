#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "types.h"
#include "culling/culling.h"
#include "culling/late_instances_culling/msoc/msoc.h"

layout(push_constant) uniform PushContants {
  uint instances_number;

  uint tiled_buffer_image_id;
  float tiled_buffer_scale_x;
  float tiled_buffer_scale_y;

  uint screen_width;
  uint screen_height;

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

layout(set = BINDLESS_SET, binding = TEXTURES_BINDING) uniform sampler2D SampledStorageImages[];
#define TiledBuffer SampledStorageImages[buffers_data.tiled_buffer_image_id]

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

float read_tile_depth(sampler2D tiled_buffer, vec2 tiled_buffer_scale, vec4 screen_rect,
                      uint tile_x, uint tile_y, uint pyramid_width, uint pyramid_height)
{
  vec2 tile_size_uv = vec2(
    float(TILE_SIZE) / float(pyramid_width),
    float(TILE_SIZE) / float(pyramid_height)
  );

  vec2 tile_center = screen_rect.xy +
    vec2(float(tile_x) + 0.5, float(tile_y) + 0.5) * tile_size_uv;

  return texture(tiled_buffer, tile_center * tiled_buffer_scale).r;
}

bool test_tile(uint instance_id, uint local_tile_id, float query_depth)
{
  TileDim dims = tile_dims[instance_id];
  uint tile_x = local_tile_id % dims.tiles_w;
  uint tile_y = local_tile_id / dims.tiles_w;

  vec2 tiled_buffer_scale = vec2(buffers_data.tiled_buffer_scale_x, buffers_data.tiled_buffer_scale_y);
  float occluder_depth = read_tile_depth(TiledBuffer, tiled_buffer_scale, screen_rects[instance_id],
                                         tile_x, tile_y,
                                         buffers_data.screen_width, buffers_data.screen_height);
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
