#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "culling/culling.h"

layout(push_constant) uniform PushContants {
  uint meshes_number;
  uint meshes_data_buffer_id;
  uint draw_commands_buffer_id;
  uint meshes_render_info_buffer_id;

  uint counters_buffer_id;
  uint draw_count_index;
  uint draw_visibility_buffer_id;
  uint draw_prefix_buffer_id;
} buffers_data;

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer MeshCullingDatasBuffer {
  MeshCullingData[] data;
} MeshCullingDatas[];
#define meshes_data MeshCullingDatas[buffers_data.meshes_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer DrawCommandsBuffer {
  IndirectCommand[] data;
} DrawCommands[];
#define draw_commands DrawCommands[buffers_data.draw_commands_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer CountersBuffer {
  uint data[];
} Counters[];
#define draws_count Counters[buffers_data.counters_buffer_id].data[buffers_data.draw_count_index]
#define instances_count(index) Counters[buffers_data.counters_buffer_id].data[index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DrawVisibilityBuffer {
  uint data[];
} DrawVisibilityBuffers[];
#define draw_visibility DrawVisibilityBuffers[buffers_data.draw_visibility_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DrawPrefixBuffer {
  uint data[];
} DrawPrefixBuffers[];
#define draw_prefix DrawPrefixBuffers[buffers_data.draw_prefix_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer DrawInfoBuffer {
  MeshDrawInfo data[];
} DrawInfosArray[];
#define meshes_draw_info DrawInfosArray[buffers_data.meshes_render_info_buffer_id].data

void fill_command(uint draw_id, uint id, uint instance_number)
{
  draw_commands[draw_id].index_count = meshes_data[id].command.index_count;
  draw_commands[draw_id].instance_count = instance_number;
  draw_commands[draw_id].first_index = meshes_data[id].command.first_index;
  draw_commands[draw_id].vertex_offset = meshes_data[id].command.vertex_offset;
  draw_commands[draw_id].first_instance = meshes_data[id].command.first_instance;

  meshes_draw_info[draw_id] = meshes_data[id].mesh_draw_info;
}

void main()
{
  uint id = gl_LocalInvocationID.x + gl_WorkGroupID.x * THREADS_NUM;
  if (id >= buffers_data.meshes_number) {
    return;
  }

#ifdef DISABLE_CULLING
  if (id == 0) {
    draws_count = buffers_data.meshes_number;
  }
  fill_command(id, id, meshes_data[id].command.instance_count);
#else // DISABLE_CULLING

  // Calculate total number of draws
  if (id == 0u) {
    if (buffers_data.meshes_number == 0u) {
      draws_count = 0u;
    } else {
      uint last = buffers_data.meshes_number - 1u;
      draws_count = draw_prefix[last] + draw_visibility[last];
    }
  }

  if (draw_visibility[id] == 0) {
    return;
  }

  uint draw_id = draw_prefix[id];
  uint instance_number = instances_count(meshes_data[id].instance_counter_index);
  fill_command(draw_id, id, instance_number);

#endif // DISABLE_CULLING
}
