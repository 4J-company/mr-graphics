#version 460

#extension GL_EXT_nonuniform_qualifier : enable

#include "culling/culling.h"

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform PushContants {
  uint instances_number;
  uint tile_count_buffer_id;
  uint tile_offset_buffer_id;
  uint counters_buffer_id;
  uint msoc_dispatch_cmd_index;
  uint culling_stat_buffer_id;
} buffers_data;

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer TileCountBuffer {
  uint data[];
} TileCountBuffers[];
#define tile_counts TileCountBuffers[buffers_data.tile_count_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer TileOffsetBuffer {
  uint data[];
} TileOffsetBuffers[];
#define tile_offsets TileOffsetBuffers[buffers_data.tile_offset_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer CountersBuffer {
  uint data[];
} Counters[];
#define counters Counters[buffers_data.counters_buffer_id].data

#ifdef COLLECT_CULLING_STAT
layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer CullingStatsBuffer {
  CullingStats stat;
} CullingStatsBuffers[];
#define culling_stat CullingStatsBuffers[buffers_data.culling_stat_buffer_id].stat
#endif // COLLECT_CULLING_STAT

// Prepare indirect dispatch for tiles test
void main()
{
  uint total_tiles = 0u;
  if (buffers_data.instances_number > 0u) {
    uint last = buffers_data.instances_number - 1u;
    total_tiles = tile_offsets[last] + tile_counts[last];
  }
  tile_offsets[buffers_data.instances_number] = total_tiles;

#ifdef COLLECT_CULLING_STAT
  atomicAdd(culling_stat.msoc_tiles_cnt, total_tiles);
#endif // COLLECT_CULLING_STAT

  uint tile_workers = (total_tiles + uint(TILES_PER_THREAD) - 1u) / uint(TILES_PER_THREAD);
  uint work_groups = (tile_workers + uint(THREADS_NUM) - 1u) / uint(THREADS_NUM);

  uint base = buffers_data.msoc_dispatch_cmd_index;
  // for indirect dispatch
  counters[base + 0u] = work_groups;
  counters[base + 1u] = 1u;
  counters[base + 2u] = 1u;
}
