#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "types.h"
#include "culling/culling.h"

#ifndef BOUNDS_TYPE
#define BOUNDS_TYPE 0
#endif // BOUNDS_TYPE

#ifndef BOUNDS_TYPE_BOXES
#define BOUNDS_TYPE_BOXES 0
#endif // BOUNDS_TYPE_BOXES

#ifndef BOUNDS_TYPE_SPHERES
#define BOUNDS_TYPE_SPHERES 1
#endif // BOUNDS_TYPE_SPHERES

#ifndef BOUNDS_TYPE_DYNAMIC_BEST
#define BOUNDS_TYPE_DYNAMIC_BEST 2
#endif // BOUNDS_TYPE_DYNAMIC_BEST

layout(push_constant) uniform PushContants {
  uint mesh_culling_data_buffer_id;
  uint instances_culling_data_buffer_id;
  uint instances_number;

  uint counters_buffer_id;

  uint transforms_in_buffer_id;

  uint camera_buffer_id;
  uint bound_boxes_buffer_id;
  uint bound_spheres_buffer_id;

  uint mip_levels_number;
  uint depth_pyramid_width;
  uint depth_pyramid_heigth;

  uint depth_pyramid_image_id;

  uint depth_pyramid_mips_scales_buffer_id;

  uint culling_stat_buffer_id;
} buffers_data;

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer MeshInstanceCullingDatasBuffer {
  MeshInstanceCullingData[] data;
} MeshInstanceCullingDatas[];
#define instances_datas MeshInstanceCullingDatas[buffers_data.instances_culling_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer MeshCullingDatasBuffer {
  MeshCullingData[] data;
} MeshCullingDatas[];
#define meshes_datas MeshCullingDatas[buffers_data.mesh_culling_data_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer BoundBoxesBuffer {
  BoundBox[] data;
} BoundBoxes[];
#define bound_box(mesh_data) BoundBoxes[buffers_data.bound_boxes_buffer_id].data[mesh_data.bound_box_index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer BoundSpheresBuffer {
  BoundSphere[] data;
} BoundSpheres[];
#define bound_sphere(mesh_data) BoundSpheres[buffers_data.bound_spheres_buffer_id].data[mesh_data.bound_box_index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer CountersBuffer {
  uint data[];
} Counters[];
#define intances_count(index) Counters[buffers_data.counters_buffer_id].data[index]

// TODO(dk6): move description of camera buffer to types.h
layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform CameraBuffer {
  CameraData data;
} CameraBufferArray[];
#define camera_buffer CameraBufferArray[buffers_data.camera_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer TransformsIn {
  mat4 transforms[];
} TransformsInArray[];
#define transforms_in TransformsInArray[buffers_data.transforms_in_buffer_id].transforms

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) writeonly buffer TransformsOut {
  InstanceDrawInfo infos[];
} TransformsOutArray[];
#define out_transforms_index(mesh_data, instance_number) \
  TransformsOutArray[mesh_data.mesh_draw_info.instance_render_info_buffer_id].infos[instance_number].transforms_index

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DepthMipsScale {
// layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform DepthMipsScaleUbo {
  float scales[32];
} DepthMipScaleUniformBuffers[];
#define depth_mip_scales DepthMipScaleUniformBuffers[buffers_data.depth_pyramid_mips_scales_buffer_id].scales

layout(set = BINDLESS_SET, binding = TEXTURES_BINDING) uniform sampler2D SampledStorageImages[];
#define DepthPyramid SampledStorageImages[buffers_data.depth_pyramid_image_id]

#ifdef COLLECT_CULLING_STAT
layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer CullingStatsBuffer {
  CullingStats stat;
} CullingStatsBuffers[];
#define culling_stat CullingStatsBuffers[buffers_data.culling_stat_buffer_id].stat
#endif // COLLECT_CULLING_STAT

float read_hiz_depth(vec4 rectangle)
{
  rectangle = clamp(rectangle, vec4(-1), vec4(1));

  vec2 rectangle_size = rectangle.zw - rectangle.xy;
  // It is very important to exact pick rectangle center - when minmax sampler will fetch all 2x2 quad data
  vec2 rectangle_center = rectangle.xy + (rectangle_size / 2);

  // Find mip level where bound rectangle is covered by 2x2 pixel quad
  float level = floor(log2(max(
    rectangle_size.x * buffers_data.depth_pyramid_width,
    rectangle_size.y * buffers_data.depth_pyramid_heigth
  )));

  vec2 mip_scale = vec2(depth_mip_scales[2 * uint(level)], depth_mip_scales[2 * uint(level) + 1]);
  vec2 tex_coord = rectangle_center * mip_scale;

  // Using max sampler get max depth of 2x2 quad which covers rectangle
  float old_depth = textureLod(DepthPyramid, tex_coord, level).r;

  return old_depth;
}

float get_bound_box_depth(BoundBox bb)
{
  vec3 bb_center = (bb.min.xyz + bb.max.xyz) / 2;

  vec3 dir_to_cam = normalize(camera_buffer.pos.xyz - bb_center);
  float bb_size = length(bb.max.xyz - bb.min.xyz) / 2;
  vec3 closest_bb_point = bb_center + dir_to_cam * bb_size;

  vec4 projected = (camera_buffer.vp * vec4(closest_bb_point, 1));
  float new_depth = projected.z / projected.w;

  return new_depth;
}

// We have 3 variants to get closest point, there is example:
// 1. Get world-space closest point, but it can be not a closest at clip space. For example:
//    campos = (0, 0, 0), camdir = (0, 0, 1).
//    ┌───────┬───────────┬──────────────────────┬───────────┐
//    │ Point │ Position  │ World space distance │ Depth (z) │
//    ├───────┼───────────┼──────────────────────┼───────────┤
//    │ A     │ (0, 0, 4) │ 4.0                  │ 4         │
//    │ B     │ (3, 0, 3) │ ≈ 4.24               │ 3         │
//    └───────┴───────────┴──────────────────────┴───────────┘
// 2. Project each point and get minimum depth - it is not always true
//    For example object is... a cube, his bound box equal itself.
//    CLO = Closest Object Point, depth - calculated depth
//    CLO < depth - depth is not minimum of distances
//       camera ●
//            / |
//     depth /  | COP
//          /   |
//         ●----●----●
//         |         |
//         |         |
//         ●----●----●
// 3. Use a bound sphere of bound box - it is simple to determine it's closest point
// 4. Use original bound sphere of object - if it is provided

float get_bound_sphere_depth(BoundSphere bs)
{
  // TODO(dk6): try write it without matrix multiplication
  vec3 dir_to_cam = normalize(camera_buffer.pos.xyz - bs.center);
  vec4 projected_center = camera_buffer.vp * vec4(bs.center + dir_to_cam * bs.radius, 1.0);
  float depth_sphere = projected_center.z / projected_center.w;
  return depth_sphere;
}

bool project_sphere(BoundSphere bs, out vec4 rectangle)
{
  vec3 center_in_view = (camera_buffer.view * vec4(bs.center, 1)).xyz;
  float znear = camera_buffer.near;
  float p00 = camera_buffer.proj[0][0];
  float p11 = camera_buffer.proj[1][1];
  return get_bound_sphere_screen_rectangle(center_in_view, bs.radius, znear, p00, p11, rectangle);
}

bool is_visible_bound_box(BoundBox bb, BoundSphere bs)
{
  if (!is_bound_sphere_dont_clips_camera((camera_buffer.view * vec4(bs.center, 1)).xyz,
                                         bs.radius, camera_buffer.near)) {
    return true;
  }

  vec4 rectangle = get_bound_box_screen_rectangle(bb, camera_buffer.vp);
  float old_depth = read_hiz_depth(rectangle);

  float new_depth = get_bound_sphere_depth(bs);
  return new_depth <= old_depth;

  // float new_depth = get_bound_box_depth(bb);
  // return new_depth >= 1 || new_depth <= old_depth;
}

bool is_visible_bound_sphere(BoundSphere bs)
{
  vec4 rectangle;
  if (!project_sphere(bs, rectangle)) {
    return true;
  }
  float old_depth = read_hiz_depth(rectangle);
  float new_depth = get_bound_sphere_depth(bs);
  return new_depth <= old_depth;
}

void main()
{
  uint id = gl_LocalInvocationID.x + gl_WorkGroupID.x * THREADS_NUM;
  if (id >= buffers_data.instances_number) {
    return;
  }

#ifdef COLLECT_CULLING_STAT
  atomicAdd(culling_stat.total_objects_cnt, 1);
#endif // COLLECT_CULLING_STAT

  // -------------------------------------
  // Frustum culling
  // -------------------------------------

  MeshInstanceCullingData instance_data = instances_datas[id];
  MeshCullingData mesh_data = meshes_datas[instance_data.mesh_culling_data_index];

  mat4 transfrom = transpose(transforms_in[instance_data.transform_index]);
  BoundBox bb = transform_bound_box(bound_box(mesh_data), transfrom);

  if (IS_INSTANCE_FRUSTUM_CALCULATED(instance_data.visibility_bits)) {
    if (!IS_INSTANCE_IN_FRUSTUM(instance_data.visibility_bits)) {
#ifdef COLLECT_CULLING_STAT
      atomicAdd(culling_stat.outside_frustum_objects_number, 1);
#endif // COLLECT_CULLING_STAT
      return;
    }
  } else {
    // Object was occluded at last frame so at first phase of current phase frustum visibility wasn't calculated
    if (!is_bound_box_frustum_visible(bb, camera_buffer.frustum_planes)) {
      // Clear occluded bit - not this object will be handled by first pass until it become in frustum
      instances_datas[id].visibility_bits = SET_INSTANCE_WAS_OCCLUDED(instance_data.visibility_bits, false);
#ifdef COLLECT_CULLING_STAT
      atomicAdd(culling_stat.outside_frustum_objects_number, 1);
#endif // COLLECT_CULLING_STAT
      return;
    }
  }

  // -------------------------------------
  // Occlussion culling
  // -------------------------------------

  bool visible = true;
  BoundSphere bs = transform_bound_sphere(bound_sphere(mesh_data), transfrom);

#if (BOUNDS_TYPE == BOUNDS_TYPE_BOXES)
  visible = is_visible_bound_box(bb, bs);
#elif (BOUNDS_TYPE == BOUNDS_TYPE_SPHERES)
  visible = is_visible_bound_sphere(bs);
#elif (BOUNDS_TYPE == BOUNDS_TYPE_DYNAMIC_BEST)
  visible = is_visible_bound_box(bb, bs) && is_visible_bound_sphere(bs);
#endif // BOUNDS_TYPE choose

  instances_datas[id].visibility_bits = SET_INSTANCE_WAS_OCCLUDED(instance_data.visibility_bits, !visible);

#ifdef COLLECT_CULLING_STAT
  atomicAdd(culling_stat.occluded_objects_cnt, visible ? 0 : 1);
#endif // COLLECT_CULLING_STAT

  if (!visible || IS_INSTANCE_RENDERER_AT_FIRST_PASS(instance_data.visibility_bits)) {
    // If object was visible it has been already rendered in first pass
    return;
  }

  // After all culling tests we still here - object is visible
  uint instance_number = atomicAdd(intances_count(mesh_data.instance_counter_index), 1);
  out_transforms_index(mesh_data, instance_number) = instance_data.transform_index;
}
