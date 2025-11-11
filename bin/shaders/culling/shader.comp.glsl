#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

// TODO(dk6): after it will moves to buffer
layout(push_constant) uniform PushContants {
  uint commands_buffer_id;
  uint draws_buffer_id;
  uint count_buffer_id;
  uint commands_number;
} buffers_data;

struct IndirectCommand {
  uint indexCount;
  uint instanceCount;
  uint firstIndex;
  int  vertexOffset;
  uint firstInstance;
};

struct DrawIndirectIndexedCommand {
  IndirectCommand command;
  vec4 minBB;
  vec4 maxBB;
};

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DrawCommandsBuffer {
  DrawIndirectIndexedCommand[] data;
} DrawCommands[];
#define commands DrawCommands[buffers_data.commands_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer DrawsBuffer {
  IndirectCommand[] data;
} Draws[];
#define draws Draws[buffers_data.draws_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer DrawsCountBuffer {
  uint data;
} DrawsCount[];
#define draws_count DrawsCount[buffers_data.count_buffer_id].data

void main()
{
  uint command_id = gl_LocalInvocationID.x + gl_WorkGroupID.x * THREADS_NUM;
  if (command_id >= buffers_data.commands_number) {
    return;
  }

  uint draw_id = atomicAdd(draws_count, 1);

  draws[draw_id].indexCount = commands[command_id].command.indexCount;
  draws[draw_id].instanceCount = commands[command_id].command.instanceCount;
  draws[draw_id].firstIndex = commands[command_id].command.firstIndex;
  draws[draw_id].vertexOffset = commands[command_id].command.vertexOffset;
  draws[draw_id].firstInstance = commands[command_id].command.firstInstance;
}