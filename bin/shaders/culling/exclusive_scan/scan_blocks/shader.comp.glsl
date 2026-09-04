#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = SCAN_BLOCKS_LOCAL_SIZE_X, local_size_y = 1, local_size_z = 1) in;

#include "culling/culling.h"

layout(push_constant) uniform PushContants {
  uint elements_number;
  uint input_buffer_id;
  uint output_buffer_id;
  uint block_sums_buffer_id;
} buffers_data;

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer InputBuffer {
  uint data[];
} InputBuffers[];
#define input_data InputBuffers[buffers_data.input_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer OutputBuffer {
  uint data[];
} OutputBuffers[];
#define output_data OutputBuffers[buffers_data.output_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer BlockSumsBuffer {
  uint data[];
} BlockSumsBuffers[];
#define block_sums BlockSumsBuffers[buffers_data.block_sums_buffer_id].data

shared uint shared_data[SCAN_BLOCK_SIZE];

void main()
{
  const uint local_id = gl_LocalInvocationID.x;
  const uint group_id = gl_WorkGroupID.x;
  const uint base = group_id * SCAN_BLOCK_SIZE;

  uint i0 = base + local_id;
  uint i1 = i0 + SCAN_BLOCKS_LOCAL_SIZE_X;

  shared_data[local_id] = i0 < buffers_data.elements_number ? input_data[i0] : 0u;
  shared_data[local_id + SCAN_BLOCKS_LOCAL_SIZE_X] = i1 < buffers_data.elements_number ? input_data[i1] : 0u;
  barrier();

  for (uint offset = 1u; offset < SCAN_BLOCK_SIZE; offset <<= 1u) {
    uint index = ((local_id + 1u) << 1u) * offset - 1u;
    if (index < SCAN_BLOCK_SIZE) {
      shared_data[index] += shared_data[index - offset];
    }
    barrier();
  }

  if (local_id == 0u) {
    uint total = shared_data[SCAN_BLOCK_SIZE - 1u];
    shared_data[SCAN_BLOCK_SIZE - 1u] = 0u;
    if (buffers_data.block_sums_buffer_id != 0xffffffffu) {
      block_sums[group_id] = total;
    }
  }
  barrier();

  for (uint offset = SCAN_BLOCKS_LOCAL_SIZE_X; offset > 0u; offset >>= 1u) {
    uint index = ((local_id + 1u) << 1u) * offset - 1u;
    if (index < SCAN_BLOCK_SIZE) {
      uint left = shared_data[index - offset];
      shared_data[index - offset] = shared_data[index];
      shared_data[index] += left;
    }
    barrier();
  }

  if (i0 < buffers_data.elements_number) {
    output_data[i0] = shared_data[local_id];
  }
  if (i1 < buffers_data.elements_number) {
    output_data[i1] = shared_data[local_id + SCAN_BLOCKS_LOCAL_SIZE_X];
  }
}
