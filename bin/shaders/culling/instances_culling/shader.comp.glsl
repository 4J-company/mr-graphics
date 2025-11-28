#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform PushContants {
  uint mesh_culling_data_buffer_id;
  uint instances_culling_data_buffer_id;
  uint instances_number;

  uint counters_buffer_id;

  uint transforms_in_buffer_id;
  uint transforms_out_buffer_id;

  uint camera_buffer_id;
  uint bound_boxes_buffer_id;
} buffers_data;

struct MeshInstanceCullingData {
  uint transform_index;
  uint mesh_culling_data_index;
};

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

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer MeshInstanceCullingDatasBuffer {
  MeshInstanceCullingData[] data;
} MeshInstanceCullingDatas[];
#define instances_datas MeshInstanceCullingDatas[buffers_data.instances_culling_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer MeshCullingDatasBuffer {
  MeshCullingData[] data;
} MeshCullingDatas[];
#define meshes_datas MeshCullingDatas[buffers_data.mesh_culling_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer BoudBoxesBuffer {
  BoundBox[] data;
} BoundBoxes[];
#define bb(mesh_data) BoundBoxes[buffers_data.bound_boxes_buffer_id].data[mesh_data.bound_box_index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer CountersBuffer {
  uint data[];
} Counters[];
#define intances_count(index) Counters[buffers_data.counters_buffer_id].data[index]

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

void main()
{
  uint id = gl_LocalInvocationID.x + gl_WorkGroupID.x * THREADS_NUM;
  if (id >= buffers_data.instances_number) {
    return;
  }

  MeshInstanceCullingData instance_data = instances_datas[id];
  MeshCullingData mesh_data = meshes_datas[instance_data.mesh_culling_data_index];
  uint transforms_start = mesh_data.mesh_draw_info.instance_offset;

// #ifdef DISABLE_CULLING
// 
//   if (id == transforms_start) {
//     intances_count(mesh_data.instance_counter_index) = mesh_data.command.instance_count;
//   }
// 
// #else // DISABLE_CULLING

  vec3 minBB = bb(mesh_data).minBB.xyz;
  vec3 maxBB = bb(mesh_data).maxBB.xyz;
  mat4 transfrom = transpose(transforms_in[instance_data.transform_index]);

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
      uint instance_number = atomicAdd(intances_count(mesh_data.instance_counter_index), 1);
      transforms_out[transforms_start + instance_number] = transforms_in[instance_data.transform_index];
      break;
    }
  }
// #endif // DISABLE_CULLING
}
