#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform PushContants {
  uint commands_buffer_id;
  uint draws_buffer_id;
  uint count_buffer_id;
  uint commands_number;
  uint camera_buffer_id;
  uint transforms_in_buffer_id;
  uint transforms_out_buffer_id;
  uint meshes_render_info_buffer_id;
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

struct DrawCommandData {
  IndirectCommand command;
  vec4 minBB;
  vec4 maxBB;
  uint transform_first_index;
  MeshDrawInfo mesh_draw_info;
};

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DrawCommandsBuffer {
  DrawCommandData[] data;
} DrawCommands[];
#define commands DrawCommands[buffers_data.commands_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer DrawsBuffer {
  IndirectCommand[] data;
} Draws[];
#define draws Draws[buffers_data.draws_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer DrawsCountBuffer {
  uint draws_count;
} DrawsCount[];
#define draws_count DrawsCount[buffers_data.count_buffer_id].draws_count

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform CameraBuffer {
  mat4 vp;
  vec4 pos;
  float fov;
  float gamma;
  float speed;
  float sens;
} CameraBufferArray[];
#define camera_buffer CameraBufferArray[buffers_data.camera_buffer_id]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer TransformsIn {
  mat4 transforms[];
} TransformsInArray[];
#define transforms_in TransformsInArray[buffers_data.transforms_in_buffer_id].transforms

// Require be same size as transforms_in
layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer TransformsOut {
  mat4 transforms[];
} TransformsOutArray[];
#define transforms_out TransformsOutArray[buffers_data.transforms_out_buffer_id].transforms

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer DrawInfoBuffers {
  MeshDrawInfo meshes_draw_info[];
} DrawInfosArray[];
#define meshes_draw_info DrawInfosArray[buffers_data.meshes_render_info_buffer_id].meshes_draw_info

bool is_point_visible(vec3 point)
{
  vec4 projected = camera_buffer.vp * vec4(point, 1);
  if (projected.w <= 0.0) {
    return false;
  }
  vec3 v = projected.xyz / projected.w;
  return
    v.x >= -1 && v.x <= 1 &&
    v.y >= -1 && v.y <= 1 &&
    v.z >= -1 && v.z <= 1; // TODO: maybe not correct formula for z
}

uint frustum_culling(uint command_id)
{
  vec3 minBB = commands[command_id].minBB.xyz;
  vec3 maxBB = commands[command_id].maxBB.xyz;
  uint transforms_start = commands[command_id].transform_first_index;

  uint visible_instances_number = 0;
  for (uint instance = 0; instance < commands[command_id].command.instance_count; instance++) {
    mat4 transfrom = transpose(transforms_in[transforms_start + instance]);

    vec3 v[8];
    v[0] = (transfrom * vec4(minBB.x, minBB.y, minBB.z, 1)).xyz;
    v[1] = (transfrom * vec4(maxBB.x, minBB.y, minBB.z, 1)).xyz;
    v[2] = (transfrom * vec4(maxBB.x, maxBB.y, minBB.z, 1)).xyz;
    v[3] = (transfrom * vec4(minBB.x, maxBB.y, minBB.z, 1)).xyz;
    v[4] = (transfrom * vec4(minBB.x, minBB.y, maxBB.z, 1)).xyz;
    v[5] = (transfrom * vec4(maxBB.x, minBB.y, maxBB.z, 1)).xyz;
    v[6] = (transfrom * vec4(maxBB.x, maxBB.y, maxBB.z, 1)).xyz;
    v[7] = (transfrom * vec4(minBB.x, maxBB.y, maxBB.z, 1)).xyz;

    for (int i = 0; i < 8; i++) {
      if (is_point_visible(v[i])) {
        // TODO(dk6): Use instead mvp here. Not, what at mesh vertex shader we also need
        //            clear transform matrix too for calculation corect real world position for shading
        transforms_out[transforms_start + visible_instances_number++] = transforms_in[transforms_start + instance];
        break;
      }
    }
  }

  return visible_instances_number;
}

void fill_command(uint draw_id, uint command_id, uint instance_number)
{
  draws[draw_id].index_count = commands[command_id].command.index_count;
  draws[draw_id].instance_count = instance_number;
  draws[draw_id].first_index = commands[command_id].command.first_index;
  draws[draw_id].vertex_offset = commands[command_id].command.vertex_offset;
  draws[draw_id].first_instance = commands[command_id].command.first_instance;

  meshes_draw_info[draw_id] = commands[command_id].mesh_draw_info;
}

void main()
{
  uint command_id = gl_LocalInvocationID.x + gl_WorkGroupID.x * THREADS_NUM;
  if (command_id >= buffers_data.commands_number) {
    return;
  }

#ifdef DISABLE_CULLING
  if (command_id == 0) {
    draws_count = buffers_data.commands_number;
  }
  fill_command(command_id, command_id, commands[command_id].command.instance_count);
#else // DISABLE_CULLING
  uint instance_number = frustum_culling(command_id);
  if (instance_number > 0) {
    uint draw_id = atomicAdd(draws_count, 1);
    fill_command(draw_id, command_id, instance_number);
  }
#endif // DISABLE_CULLING
}
