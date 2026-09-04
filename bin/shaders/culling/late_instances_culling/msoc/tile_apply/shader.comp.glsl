#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "types.h"
#include "culling/culling.h"
#include "culling/late_instances_culling/msoc/msoc.h"

layout(push_constant) uniform PushContants {
  uint mesh_culling_data_buffer_id;
  uint instances_culling_data_buffer_id;
  uint instances_number;

  uint counters_buffer_id;

  uint transforms_in_buffer_id;

  uint camera_buffer_id;
  uint bound_boxes_buffer_id;
  uint bound_spheres_buffer_id;

  uint tile_count_buffer_id;
  uint visible_flag_buffer_id;

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

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer TileCountBuffer {
  uint data[];
} TileCountBuffers[];
#define tile_counts TileCountBuffers[buffers_data.tile_count_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer VisibleFlagBuffer {
  uint data[];
} VisibleFlagBuffers[];
#define visible_flags VisibleFlagBuffers[buffers_data.visible_flag_buffer_id].data

#ifdef COLLECT_CULLING_STAT
layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer CullingStatsBuffer {
  CullingStats stat;
} CullingStatsBuffers[];
#define culling_stat CullingStatsBuffers[buffers_data.culling_stat_buffer_id].stat
#endif // COLLECT_CULLING_STAT

void main()
{
  uint id = gl_LocalInvocationID.x + gl_WorkGroupID.x * THREADS_NUM;
  if (id >= buffers_data.instances_number) {
    return;
  }

  if (tile_counts[id] == 0u) {
    return;
  }

  MeshInstanceCullingData instance_data = instances_datas[id];
  MeshCullingData mesh_data = meshes_datas[instance_data.mesh_culling_data_index];

  mat4 transfrom = transpose(transforms_in[instance_data.transform_index]);
  BoundBox bb = transform_bound_box(bound_box(mesh_data), transfrom);
  BoundSphere bs = transform_bound_sphere(bound_sphere(mesh_data), transfrom);

  bool visible = visible_flags[id] != 0u;
  if (!visible && is_near_clip_visible(camera_buffer, bs)) {
    visible = true;
  }

  instances_datas[id].visibility_bits = SET_INSTANCE_WAS_OCCLUDED(instance_data.visibility_bits, !visible);

#ifdef COLLECT_CULLING_STAT
  atomicAdd(culling_stat.occluded_objects_cnt, visible ? 0 : 1);
#endif // COLLECT_CULLING_STAT

  if (!IS_INSTANCE_RENDERER_AT_FIRST_PASS(instance_data.visibility_bits) && visible) {
    uint instance_number = atomicAdd(intances_count(mesh_data.instance_counter_index), 1);
    out_transforms_index(mesh_data, instance_number) = instance_data.transform_index;
  }
}
