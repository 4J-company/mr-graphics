#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "culling/culling.h"

struct DepthPyramid {
  uint levels_number;
  uint levels_ids[MAX_DEPTH_PYRAMID_LEVELS];
};

layout(push_constant) uniform PushContants {
  uint mesh_culling_data_buffer_id;
  uint instances_culling_data_buffer_id;
  uint instances_number;

  uint counters_buffer_id;

  uint transforms_in_buffer_id;
  uint transforms_out_buffer_id;

  uint camera_buffer_id;
  uint bound_boxes_buffer_id;

  uint depth_pyramid_data_buffer_id;

  uvec2 depth_pyramid_size;
} buffers_data;

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer MeshInstanceCullingDatasBuffer {
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

// TODO(dk6): move description of camera buffer to types.h
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

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform DepthPyramidDataBuffer {
  DepthPyramid data;
} DepthPyramidData[];
#define depth_pyramid_data DepthPyramidData[buffers_data.depth_pyramid_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_IMAGES_BINDING, r32f) uniform image2D StorageImages[];
#define DepthLevel(level) StorageImages[depth_pyramid_data.levels_ids[level]]

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
  BoundBox bb = transform_bound_box(bound_box(mesh_data), transfrom);

  for (int i = 0; i < 6; i++) {
    vec4 plane = camera_buffer.frustum_planes[i];
    if (is_bound_box_not_visible(plane, bb)) {
      return;
    }
  }

  // -------------------------------------
  // Occlussion culling
  // -------------------------------------

  vec4 rectangle = get_bound_box_screen_rectangle(bound_box(mesh_data), camera_buffer.vp);
  // TODO(dk6): maybe here correct '*= -1' y components
  // rectangle now in [-1; 1] screen coords, not in pixels - we must convert it to [0; w] and [0; h]
  rectangle.x = ((rectangle.x + 1) / 2) * buffers_data.depth_pyramid_size.x;
  rectangle.y = ((rectangle.y + 1) / 2) * buffers_data.depth_pyramid_size.y;
  rectangle.z = ((rectangle.z + 1) / 2) * buffers_data.depth_pyramid_size.x;
  rectangle.w = ((rectangle.w + 1) / 2) * buffers_data.depth_pyramid_size.y;

  float rectangle_width = rectangle.z - rectangle.x;
  float rectangle_height = rectangle.w - rectangle.y;
  vec2 rectangle_center = (rectangle.xy + rectangle.zw) / 2;
  vec3 bb_center = (bb.min.xyz + bb.max.xyz) / 2;

  int level = int(floor(log2(max(rectangle_width, rectangle_height))));
  rectangle_center /= (level + 1);
  // TODO(dk6): maybe get 4 pixels? Also it is better to use sampler for average value
  float old_depth = imageLoad(DepthLevel(level), ivec2(rectangle_center)).r;
  float new_depth = (camera_buffer.vp * vec4(bb_center, 1)).z;

  if (new_depth > old_depth) {
    return;
  }

  // After all culling tests we still here - object is visible
  uint instance_number = atomicAdd(intances_count(mesh_data.instance_counter_index), 1);
  transforms_out[transforms_start + instance_number] = transforms_in[instance_data.transform_index];

  instances_datas[id].visible_last_frame = 1; // Save for next frame
}
