#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = SCAN_BLOCKS_LOCAL_SIZE_X, local_size_y = 1, local_size_z = 1) in;

#include "culling/culling.h"

layout(push_constant) uniform PushContants {
  uint elements_number;
  uint data_buffer_id;
  uint block_offsets_buffer_id;
} buffers_data;

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer DataBuffer {
  uint data[];
} DataBuffers[];
#define data_array DataBuffers[buffers_data.data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer BlockOffsetsBuffer {
  uint data[];
} BlockOffsetsBuffers[];
#define block_offsets BlockOffsetsBuffers[buffers_data.block_offsets_buffer_id].data

void main()
{
  const uint local_id = gl_LocalInvocationID.x;
  const uint group_id = gl_WorkGroupID.x;
  const uint base = group_id * SCAN_BLOCK_SIZE;
  const uint offset = block_offsets[group_id];

  uint i0 = base + local_id;
  uint i1 = i0 + SCAN_BLOCKS_LOCAL_SIZE_X;

  if (i0 < buffers_data.elements_number) {
    data_array[i0] += offset;
  }
  if (i1 < buffers_data.elements_number) {
    data_array[i1] += offset;
  }
}
