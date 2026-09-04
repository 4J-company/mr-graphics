#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "types.h"
#include "culling/culling.h"
#include "culling/late_instances_culling/msoc/msoc.h"

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

layout(push_constant) uniform PushContants {
  uint mesh_culling_data_buffer_id;
  uint instances_culling_data_buffer_id;
  uint instances_number;

  uint transforms_in_buffer_id;

  uint camera_buffer_id;
  uint bound_boxes_buffer_id;
  uint bound_spheres_buffer_id;

  uint screen_width;
  uint screen_height;

  uint tile_count_buffer_id;
  uint tile_dims_buffer_id;
  uint screen_rect_buffer_id;
  uint query_depth_buffer_id;

  uint culling_stat_buffer_id;
} buffers_data;

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer MeshInstanceCullingDatasBuffer {
  MeshInstanceCullingData[] data;
} MeshInstanceCullingDatas[];
#define instances_datas MeshInstanceCullingDatas[buffers_data.instances_culling_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer MeshCullingDatasBuffer {
  MeshCullingData[] data;
} MeshCullingDatas[];
#define meshes_datas MeshCullingDatas[buffers_data.mesh_culling_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer BoundBoxesBuffer {
  BoundBox[] data;
} BoundBoxes[];
#define bound_box(mesh_data) BoundBoxes[buffers_data.bound_boxes_buffer_id].data[mesh_data.bound_box_index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer BoundSpheresBuffer {
  BoundSphere[] data;
} BoundSpheres[];
#define bound_sphere(mesh_data) BoundSpheres[buffers_data.bound_spheres_buffer_id].data[mesh_data.bound_box_index]

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform CameraBuffer {
  CameraData data;
} CameraBufferArray[];
#define camera_buffer CameraBufferArray[buffers_data.camera_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer TransformsIn {
  mat4 transforms[];
} TransformsInArray[];
#define transforms_in TransformsInArray[buffers_data.transforms_in_buffer_id].transforms

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer TileCountBuffer {
  uint data[];
} TileCountBuffers[];
#define tile_counts TileCountBuffers[buffers_data.tile_count_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer TileDimsBuffer {
  TileDim data[];
} TileDimsBuffers[];
#define tile_dims TileDimsBuffers[buffers_data.tile_dims_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer ScreenRectBuffer {
  vec4 data[];
} ScreenRectBuffers[];
#define screen_rects ScreenRectBuffers[buffers_data.screen_rect_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer QueryDepthBuffer {
  float data[];
} QueryDepthBuffers[];
#define query_depths QueryDepthBuffers[buffers_data.query_depth_buffer_id].data

#ifdef COLLECT_CULLING_STAT
layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer CullingStatsBuffer {
  CullingStats stat;
} CullingStatsBuffers[];
#define culling_stat CullingStatsBuffers[buffers_data.culling_stat_buffer_id].stat
#endif // COLLECT_CULLING_STAT

vec4 get_screen_rectangle(BoundBox bb, BoundSphere bs)
{
#if (BOUNDS_TYPE == BOUNDS_TYPE_BOXES)
  return get_bound_box_screen_rectangle(bb, camera_buffer.vp);
#elif (BOUNDS_TYPE == BOUNDS_TYPE_SPHERES)
  vec4 rectangle;
  if (!project_sphere(camera_buffer, bs, rectangle)) {
    return vec4(0);
  }
  return rectangle;
#elif (BOUNDS_TYPE == BOUNDS_TYPE_DYNAMIC_BEST)
  vec4 bs_rect;
  if (!project_sphere(camera_buffer, bs, bs_rect)) {
    return get_bound_box_screen_rectangle(bb, camera_buffer.vp);
  }
  vec4 bb_rect = get_bound_box_screen_rectangle(bb, camera_buffer.vp);
  float bb_rect_size = bounds_rectangle_max_dim(bb_rect);
  float bs_rect_size = bounds_rectangle_max_dim(bs_rect);
  return bb_rect_size < bs_rect_size ? bb_rect : bs_rect;
#elif (BOUNDS_TYPE == BOUNDS_TYPE_BOTH)
  // Cover union of both projections so tile grid includes either bound.
  vec4 bs_rect;
  if (!project_sphere(camera_buffer, bs, bs_rect)) {
    return get_bound_box_screen_rectangle(bb, camera_buffer.vp);
  }
  vec4 bb_rect = get_bound_box_screen_rectangle(bb, camera_buffer.vp);
  float bb_rect_size = bounds_rectangle_max_dim(bb_rect);
  float bs_rect_size = bounds_rectangle_max_dim(bs_rect);
  return bb_rect_size < bs_rect_size ? bs_rect : bb_rect;
#else
  return get_bound_box_screen_rectangle(bb, camera_buffer.vp);
#endif
}

void main()
{
  uint id = gl_LocalInvocationID.x + gl_WorkGroupID.x * THREADS_NUM;
  if (id >= buffers_data.instances_number) {
    return;
  }

#ifdef COLLECT_CULLING_STAT
  atomicAdd(culling_stat.total_objects_cnt, 1);
#endif // COLLECT_CULLING_STAT

  MeshInstanceCullingData instance_data = instances_datas[id];
  MeshCullingData mesh_data = meshes_datas[instance_data.mesh_culling_data_index];

  mat4 transfrom = transpose(transforms_in[instance_data.transform_index]);
  BoundBox bb = transform_bound_box(bound_box(mesh_data), transfrom);
  BoundSphere bs = transform_bound_sphere(bound_sphere(mesh_data), transfrom);

  if (IS_INSTANCE_FRUSTUM_CALCULATED(instance_data.visibility_bits)) {
    if (!IS_INSTANCE_IN_FRUSTUM(instance_data.visibility_bits)) {
#ifdef COLLECT_CULLING_STAT
      atomicAdd(culling_stat.outside_frustum_objects_number, 1);
#endif // COLLECT_CULLING_STAT
      tile_counts[id] = 0u;
      return;
    }
  } else {
    if (!is_bound_box_frustum_visible(bb, camera_buffer.frustum_planes)) {
      instances_datas[id].visibility_bits = SET_INSTANCE_WAS_OCCLUDED(instance_data.visibility_bits, false);
#ifdef COLLECT_CULLING_STAT
      atomicAdd(culling_stat.outside_frustum_objects_number, 1);
#endif // COLLECT_CULLING_STAT
      tile_counts[id] = 0u;
      return;
    }
  }

  vec4 rectangle = get_screen_rectangle(bb, bs);
  rectangle = clamp(rectangle, vec4(0.0), vec4(1.0));
#ifdef MSOC_SNAP_RECT_TO_TILE_GRID
  rectangle = snap_screen_rect_to_tile_grid(rectangle, buffers_data.screen_width, buffers_data.screen_height);
#endif
  screen_rects[id] = rectangle;

  uvec2 dims = compute_tile_dims(rectangle, buffers_data.screen_width, buffers_data.screen_height);
  tile_dims[id].tiles_w = dims.x;
  tile_dims[id].tiles_h = dims.y;
  tile_counts[id] = dims.x * dims.y;
  query_depths[id] = get_instance_query_depth(camera_buffer, bb, bs);
}
