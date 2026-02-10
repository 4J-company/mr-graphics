#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "culling/culling.h"

layout(push_constant) uniform PushContants {
  uint meshes_number;
  uint meshes_data_buffer_id;
  uint counters_buffer_id;
  uint draw_visibility_buffer_id;
} buffers_data;

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer MeshCullingDatasBuffer {
  MeshCullingData[] data;
} MeshCullingDatas[];
#define meshes_data MeshCullingDatas[buffers_data.meshes_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer CountersBuffer {
  uint data[];
} Counters[];
#define instances_count(index) Counters[buffers_data.counters_buffer_id].data[index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer DrawVisibilityBuffer {
  uint data[];
} DrawVisibilityBuffers[];
#define draw_visibility DrawVisibilityBuffers[buffers_data.draw_visibility_buffer_id].data

void main()
{
  uint id = gl_LocalInvocationID.x + gl_WorkGroupID.x * THREADS_NUM;
  if (id >= buffers_data.meshes_number) {
    return;
  }

  uint instance_number = instances_count(meshes_data[id].instance_counter_index);
  draw_visibility[id] = instance_number > 0 ? 1 : 0;
}
