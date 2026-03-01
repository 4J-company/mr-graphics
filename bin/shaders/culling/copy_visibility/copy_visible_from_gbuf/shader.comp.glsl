#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = THREADS_NUM, local_size_z = 1) in;
layout(push_constant) uniform PushContants {
  uint gbuf_id;
  uvec2 gbuf_size;
  uint visibility_states_buffer_id;
} data;

layout(set = BINDLESS_SET, binding = TEXTURES_BINDING) uniform sampler2D SampledImages[];
#define PosGbuf SampledImages[data.gbuf_id]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer VisibilityStatesBuffer {
  uint data[];
} VisibilityStates[];
#define visibility_states VisibilityStates[data.visibility_states_buffer_id].data

void main()
{
  uvec2 coord = gl_GlobalInvocationID.xy;
  if (coord.x >= data.dst_size.x || coord.y >= data.dst_size.y) {
    return;
  }
  coord /= data.gbuf_size;

  uint id = floatBitsToInt(texutre(PosGbuf, coord).w);
  visibility_states[id] = SET_INSTANCE_ON_SCREEN(visibility_states[id], true);
}
