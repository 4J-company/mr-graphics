#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "types.h"
#include "culling/culling.h"

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

  uint counters_buffer_id;

  uint transforms_in_buffer_id;

  uint camera_buffer_id;
  uint bound_boxes_buffer_id;
  uint bound_spheres_buffer_id;

  uint mip_levels_number;
  uint depth_pyramid_width;
  uint depth_pyramid_heigth;

  uint depth_pyramid_image_id;

  uint depth_pyramid_mips_scales_buffer_id;

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

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer CountersBuffer {
  uint data[];
} Counters[];
#define intances_count(index) Counters[buffers_data.counters_buffer_id].data[index]

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform CameraBuffer {
  CameraData data;
} CameraBufferArray[];
#define camera_buffer CameraBufferArray[buffers_data.camera_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer TransformsIn {
  mat4 transforms[];
} TransformsInArray[];
#define transforms_in TransformsInArray[buffers_data.transforms_in_buffer_id].transforms

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer TransformsOut {
  InstanceDrawInfo infos[];
} TransformsOutArray[];
#define out_transforms_index(mesh_data, instance_number) \
  TransformsOutArray[mesh_data.mesh_draw_info.instance_render_info_buffer_id].infos[instance_number].transforms_index

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DepthMipsScale {
  float scales[32];
} DepthMipScaleUniformBuffers[];
#define depth_mip_scales DepthMipScaleUniformBuffers[buffers_data.depth_pyramid_mips_scales_buffer_id].scales

layout(set = BINDLESS_SET, binding = TEXTURES_BINDING) uniform sampler2D SampledStorageImages[];
#define DepthPyramid SampledStorageImages[buffers_data.depth_pyramid_image_id]

#ifdef COLLECT_CULLING_STAT
layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer CullingStatsBuffer {
  CullingStats stat;
} CullingStatsBuffers[];
#define culling_stat CullingStatsBuffers[buffers_data.culling_stat_buffer_id].stat
#endif // COLLECT_CULLING_STAT

#include "culling/hiz.h"

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

  if (IS_INSTANCE_FRUSTUM_CALCULATED(instance_data.visibility_bits)) {
    if (!IS_INSTANCE_IN_FRUSTUM(instance_data.visibility_bits)) {
#ifdef COLLECT_CULLING_STAT
      atomicAdd(culling_stat.outside_frustum_objects_number, 1);
#endif // COLLECT_CULLING_STAT
      return;
    }
  } else {
    if (!is_bound_box_frustum_visible(bb, camera_buffer.frustum_planes)) {
      instances_datas[id].visibility_bits = SET_INSTANCE_WAS_OCCLUDED(instance_data.visibility_bits, false);
#ifdef COLLECT_CULLING_STAT
      atomicAdd(culling_stat.outside_frustum_objects_number, 1);
#endif // COLLECT_CULLING_STAT
      return;
    }
  }

  BoundSphere bs = transform_bound_sphere(bound_sphere(mesh_data), transfrom);
  bool visible = is_instance_hiz_visible(camera_buffer, bb, bs);

  instances_datas[id].visibility_bits = SET_INSTANCE_WAS_OCCLUDED(instance_data.visibility_bits, !visible);

#ifdef COLLECT_CULLING_STAT
  atomicAdd(culling_stat.occluded_objects_cnt, visible ? 0 : 1);
#endif // COLLECT_CULLING_STAT

  if (!IS_INSTANCE_RENDERER_AT_FIRST_PASS(instance_data.visibility_bits) && visible) {
    uint instance_number = atomicAdd(intances_count(mesh_data.instance_counter_index), 1);
    out_transforms_index(mesh_data, instance_number) = instance_data.transform_index;
  }
}
