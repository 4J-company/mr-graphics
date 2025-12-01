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
  vec4 min;
  vec4 max;
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
#define bound_box(mesh_data) BoundBoxes[buffers_data.bound_boxes_buffer_id].data[mesh_data.bound_box_index]

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
  vec4 frustum_planes[6];
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

BoundBox apply_transform(BoundBox bb, mat4 transform)
{
  vec3 corners[8];

  corners[0] = (transform * vec4(bb.min.x, bb.min.y, bb.min.z, 1)).xyz;
  corners[1] = (transform * vec4(bb.max.x, bb.min.y, bb.min.z, 1)).xyz;
  corners[2] = (transform * vec4(bb.max.x, bb.max.y, bb.min.z, 1)).xyz;
  corners[3] = (transform * vec4(bb.min.x, bb.max.y, bb.min.z, 1)).xyz;
  corners[4] = (transform * vec4(bb.min.x, bb.min.y, bb.max.z, 1)).xyz;
  corners[5] = (transform * vec4(bb.max.x, bb.min.y, bb.max.z, 1)).xyz;
  corners[6] = (transform * vec4(bb.max.x, bb.max.y, bb.max.z, 1)).xyz;
  corners[7] = (transform * vec4(bb.min.x, bb.max.y, bb.max.z, 1)).xyz;

  BoundBox res;
  res.min = vec4(corners[0], 0);
  res.max = vec4(corners[0], 0);
  for (uint i = 1; i < 8; i++) {
    res.min = vec4(min(res.min.xyz, corners[i]), 0);
    res.max = vec4(max(res.max.xyz, corners[i]), 0);
  }
  return res;
}

// *********************************
// This part is for debug - bound boxes are more exact
struct BoundSphere {
  vec3 center;
  float radius;
};

BoundSphere bb2bs(BoundBox bb)
{
  BoundSphere bs;
  bs.center = (bb.max.xyz + bb.min.xyz) / 2;
  bs.radius = length(bb.max.xyz - bb.min.xyz) / 2;
  return bs;
}

bool is_bound_sphere_not_visible(vec4 plane, BoundSphere bs) {
  return !(dot(plane, vec4(bs.center, 1)) > -bs.radius);
}
// *********************************

bool is_bound_box_not_visible(vec4 plane, BoundBox bb)
{
  vec3 positive = bb.min.xyz;
  vec3 negative = bb.max.xyz;

  if (plane.x >= 0) {
    positive.x = bb.max.x;
    negative.x = bb.min.x;
  }
  if (plane.y >= 0) {
    positive.y = bb.max.y;
    negative.y = bb.min.y;
  }
  if (plane.z >= 0) {
    positive.z = bb.max.z;
    negative.z = bb.min.z;
  }

  if (dot(plane.xyz, negative) + plane.w > 0) {
    // If the nearest point is outside the plane,
    //   the box is completely outside or intersects
    return false;
  }

  if (dot(plane.xyz, positive) + plane.w < 0) {
    // If the far point is beyond the plane, the box is completely outside
    return true;
  }

  return false;
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

  mat4 transfrom = transpose(transforms_in[instance_data.transform_index]);
  BoundBox bb = apply_transform(bound_box(mesh_data), transfrom);

  for (int i = 0; i < 6; i++) {
    vec4 plane = camera_buffer.frustum_planes[i];
    if (is_bound_box_not_visible(plane, bb)) {
      return;
    }
  }

  uint instance_number = atomicAdd(intances_count(mesh_data.instance_counter_index), 1);
  transforms_out[transforms_start + instance_number] = transforms_in[instance_data.transform_index];
}
