#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "culling/culling.h"

layout(push_constant) uniform PushContants {
  uint visibility_states_buffer_id;
  uint instances_number;
} buffers_data;

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
  visibility_states[id] = SET_INSTANCE_ON_SCREEN(visibility_states[id], false);
}
