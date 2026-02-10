#version 460

#extension GL_EXT_nonuniform_qualifier : enable

#include "culling/culling.h"

layout(local_size_x = THREADS_NUM, local_size_y = THREADS_NUM, local_size_z = 1) in;
layout(push_constant) uniform PushContants {
  uint gbuf_id;
  uint gbuf_width;
  uint gbuf_heigth;
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
  uvec2 gbuf_size = uvec2(data.gbuf_width, data.gbuf_heigth);
  if (coord.x >= gbuf_size.x || coord.y >= gbuf_size.y) {
    return;
  }

  vec4 pixel = texelFetch(PosGbuf, ivec2(coord), 0);
  uint id = floatBitsToUint(pixel.w);

  if (id != 0xFFFFFFFF) {
    visibility_states[id] = SET_INSTANCE_ON_SCREEN(visibility_states[id], true);
  }
}
