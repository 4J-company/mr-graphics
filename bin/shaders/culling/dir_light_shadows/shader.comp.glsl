#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "culling/culling.h"

layout(push_constant) uniform PushContants {
  uint mesh_culling_data_buffer_id;
  uint instances_culling_data_buffer_id;
  uint instances_number;
  uint counters_buffer_id;
  uint transforms_in_buffer_id;

  // TODO(dk6): for culling in feature:
  // uint light_buffer_id;
  // uint bound_boxes_buffer_id;
} buffers_data;

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer MeshInstanceCullingDatasBuffer {
  MeshInstanceCullingData[] data;
} MeshInstanceCullingDatas[];
#define instances_datas MeshInstanceCullingDatas[buffers_data.instances_culling_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer MeshCullingDatasBuffer {
  MeshCullingData[] data;
} MeshCullingDatas[];
#define meshes_datas MeshCullingDatas[buffers_data.mesh_culling_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer BoudBoxesBuffer {
  BoundBox[] data;
} BoundBoxes[];
#define bound_box(mesh_data) BoundBoxes[buffers_data.bound_boxes_buffer_id].data[mesh_data.bound_box_index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer CountersBuffer {
  uint data[];
} Counters[];
#define intances_count(index) Counters[buffers_data.counters_buffer_id].data[index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer TransformsIn {
  mat4 transforms[];
} TransformsInArray[];
#define transforms_in TransformsInArray[buffers_data.transforms_in_buffer_id].transforms

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer TransformsOut {
  mat4 transforms[];
} TransformsOutArray[];
#define transforms_out(mesh_data) TransformsOutArray[mesh_data.mesh_draw_info.transforms_buffer_id].transforms

void main()
{
  uint id = gl_LocalInvocationID.x + gl_WorkGroupID.x * THREADS_NUM;
  if (id >= buffers_data.instances_number) {
    return;
  }

  MeshInstanceCullingData instance_data = instances_datas[id];
  MeshCullingData mesh_data = meshes_datas[instance_data.mesh_culling_data_index];

  // After frustun culling tests we still here - object is visible
  uint instance_number = atomicAdd(intances_count(mesh_data.instance_counter_index), 1);
  transforms_out(mesh_data)[instance_number] = transforms_in[instance_data.transform_index];
}
