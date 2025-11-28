#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform PushContants {
  uint meshes_number;
  uint meshes_data_buffer_id;
  uint draw_commands_buffer_id;
  uint meshes_render_info_buffer_id;

  uint counters_buffer_id;
  uint draw_count_index;
} buffers_data;

struct MeshDrawInfo {
  uint mesh_offset;
  uint instance_offset;
  uint material_buffer_id;
};

struct IndirectCommand {
  uint index_count;
  uint instance_count;
  uint first_index;
  int  vertex_offset;
  uint first_instance;
};

struct MeshCullingData {
  IndirectCommand command;
  MeshDrawInfo mesh_draw_info;

  uint instance_counter_index;
  uint bound_box_index;
};

struct BoundBox {
  vec4 minBB;
  vec4 maxBB;
};

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
  uint instance_number = instances_count(meshes_data[id].instance_counter_index);
  if (instance_number > 0) {
    uint draw_id = atomicAdd(draws_count, 1);
    fill_command(draw_id, id, instance_number);
  }
#endif // DISABLE_CULLING
}
