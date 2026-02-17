#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "culling/culling.h"

layout(push_constant) uniform PushContants {
  uint instances_culling_data_buffer_id;
  uint instances_number;
  uint visibility_states_buffer_id;
  uint clear_visibility;
} buffers_data;

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer MeshInstanceCullingDatasBuffer {
  MeshInstanceCullingData[] data;
} MeshInstanceCullingDatas[];
#define instances_datas MeshInstanceCullingDatas[buffers_data.instances_culling_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer VisibilityStatesBuffer {
  uint data[];
} VisibilityStates[];
#define visibility_states VisibilityStates[buffers_data.visibility_states_buffer_id].data

void main()
{
  uint id = gl_LocalInvocationID.x + gl_WorkGroupID.x * THREADS_NUM;
  if (id >= buffers_data.instances_number) {
    return;
  }

  MeshInstanceCullingData instance_data = instances_datas[id];

  visibility_states[instance_data.transform_index] = (buffers_data.clear_visibility == 1)
    ? (SET_INSTANCE_IN_FRUSTUM(0, true) | SET_INSTANCE_WAS_OCCLUDED(0, false))
    : instance_data.visibility_bits;
}
