#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "culling/culling.h"

layout(push_constant) uniform PushContants {
  uint mesh_culling_data_buffer_id;
  uint instances_culling_data_buffer_id;
  uint instances_number;

  uint counters_buffer_id;

  uint transforms_in_buffer_id;
  uint transforms_out_buffer_id;

  uint camera_buffer_id;
  uint bound_boxes_buffer_id;

  uint mip_levels_number;
  uint depth_pyramid_data_buffer_id;
  uvec2 depth_pyramid_size;

  uint depth_pyramid_image_id;
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

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DepthPyramidDataBuffer {
  uint mips[];
} DepthPyramidData[];
#define depth_pyramid_mips DepthPyramidData[buffers_data.depth_pyramid_data_buffer_id].mips

layout(set = BINDLESS_SET, binding = STORAGE_IMAGES_BINDING, r32f) uniform image2D StorageImages[];
#define DepthLevel(level) StorageImages[depth_pyramid_mips[level]]

layout(set = BINDLESS_SET, binding = TEXTURES_BINDING) uniform sampler2D SampledStorageImages[];
#define DepthPyramid SampledStorageImages[buffers_data.depth_pyramid_image_id]

//   vec4 rectangle_inv = rectangle;
//   rectangle_inv.y = buffers_data.depth_pyramid_size.y - rectangle_inv.w;
//   rectangle_inv.w = buffers_data.depth_pyramid_size.y - rectangle_inv.y;
//
//   float rectangle_width_inv = rectangle_inv.z - rectangle_inv.x;
//   float rectangle_height_inv = rectangle_inv.w - rectangle_inv.y;
//   vec2 rectangle_center_inv = vec2(rectangle_inv.x + rectangle_width_inv / 2, rectangle_inv.y + rectangle_height_inv / 2);
//   float level_inv = floor(log2(max(rectangle_width_inv, rectangle_height_inv)));
//   float old_depth_inv = textureLod(DepthPyramid, rectangle_center_inv, level_inv).r;
//   float old_depth = max(old_depth1, old_depth_inv);

  // int ilevel = int(level);
  // vec2 level_rectangle_center = rectangle_center / pow(2, ilevel);
  // float old_depth = imageLoad(DepthLevel(ilevel), ivec2(level_rectangle_center)).r;

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

  // --- Get current depth of already visible objects ---
  BoundBox sbb = get_bound_box_screen_rectangle(bb, camera_buffer.vp);
  vec4 rectangle_screen = vec4(sbb.min.x, sbb.min.y, sbb.max.x, sbb.max.y);

  // Flip over Ox
  float tmp = -rectangle_screen.y;
  rectangle_screen.y = -rectangle_screen.w;
  rectangle_screen.w = tmp;

  // rectangle now in [-1; 1] screen coords, not in pixels - we must convert it to [0; w] and [0; h]
  vec4 rectangle;
  // rectangle.x = ((rectangle_screen.x + 1) / 2) * buffers_data.depth_pyramid_size.x;
  // rectangle.y = ((rectangle_screen.y + 1) / 2) * buffers_data.depth_pyramid_size.y;
  // rectangle.z = ((rectangle_screen.z + 1) / 2) * buffers_data.depth_pyramid_size.x;
  // rectangle.w = ((rectangle_screen.w + 1) / 2) * buffers_data.depth_pyramid_size.y;
  rectangle.x = ((rectangle_screen.x + 1) / 2);
  rectangle.y = ((rectangle_screen.y + 1) / 2);
  rectangle.z = ((rectangle_screen.z + 1) / 2);
  rectangle.w = ((rectangle_screen.w + 1) / 2);

  float rectangle_width = rectangle.z - rectangle.x;
  float rectangle_height = rectangle.w - rectangle.y;
  vec2 rectangle_center = vec2(rectangle.x + rectangle_width / 2, rectangle.y + rectangle_height / 2);
  float level = floor(log2(max(rectangle_width, rectangle_height)));

  // TODO(dk6): check specification of textureLod
  float old_depth = textureLod(DepthPyramid, rectangle_center, level).r;

  // --- Get current depth closest to cam point of bound box ---
  // vec3 bb_center = (bb.min.xyz + bb.max.xyz) / 2;
  // vec3 dir_to_cam = camera_buffer.pos.xyz - bb_center;
  // float dist_to_cam = length(dir_to_cam);
  // dir_to_cam = normalize(dir_to_cam);
  // float bb_size = length(bb.max.xyz - bb.min.xyz) / 2;
  // float optimal_len = min(bb_size, dist_to_cam * 0.99);
  // vec3 closest_bb_point = bb_center + dir_to_cam * optimal_len;
  // vec4 projected = (camera_buffer.vp * vec4(closest_bb_point, 1));
  // float new_depth = projected.z / projected.w;
  float new_depth = sbb.min.z;

  // --- Check visibility ---
  float bias = 0.001;
  bias = 0;
  // new_depth = min(new_depth, 0.9999);
  bool visible = new_depth < old_depth + bias;

  bool was_visible = instance_data.visible_last_frame == 1;
  instances_datas[id].visible_last_frame = visible ? 1 : 0;
  if (!visible || was_visible) {
    return;
  }

  // After all culling tests we still here - object is visible
  if (instance_data.visible_last_frame == 0) {
    uint instance_number = atomicAdd(intances_count(mesh_data.instance_counter_index), 1);
    transforms_out[transforms_start + instance_number] = transforms_in[instance_data.transform_index];
  }
}
