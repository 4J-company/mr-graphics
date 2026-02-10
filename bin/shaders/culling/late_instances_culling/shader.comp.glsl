#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = THREADS_NUM, local_size_y = 1, local_size_z = 1) in;

#include "types.h"
#include "culling/culling.h"

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

#define USE_BOUND_BOXES 1

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

#if USE_BOUND_BOXES
  // --- Get current depth of already visible objects ---
  vec4 rectangle_screen = get_bound_box_screen_rectangle(bb, camera_buffer.vp);
  rectangle_screen.x = clamp(rectangle_screen.x, -1, 1);
  rectangle_screen.y = clamp(rectangle_screen.y, -1, 1);
  rectangle_screen.z = clamp(rectangle_screen.z, -1, 1);
  rectangle_screen.w = clamp(rectangle_screen.w, -1, 1);

  // Flip over Ox
  float tmp = -rectangle_screen.y;
  rectangle_screen.y = -rectangle_screen.w;
  rectangle_screen.w = tmp;

  // rectangle now in [-1; 1] screen coords, convert to [0; 1] texture coords
  vec4 rectangle = (rectangle_screen + vec4(1)) / 2;

  vec2 rectangle_size = rectangle.zw - rectangle.xy;
  vec2 rectangle_center = rectangle.xy + (rectangle_size / 2);

  // Find mip level where bound rectange is covered by 2x2 pixel quad
  float level = floor(log2(max(
    rectangle_size.x * buffers_data.depth_pyramid_width,
    rectangle_size.y * buffers_data.depth_pyramid_heigth
  )));

  vec2 mip_scale = vec2(depth_mip_scales[2 * uint(level)], depth_mip_scales[2 * uint(level) + 1]);
  vec2 tex_coord = rectangle_center * mip_scale;

  // Using max sampler get max depth of 2x2 quad which covers rectangle
  float old_depth = textureLod(DepthPyramid, tex_coord, level).r;

  // --- Get current depth closest to cam point of bound box ---
  vec3 bb_center = (bb.min.xyz + bb.max.xyz) / 2;

  vec3 dir_to_cam = normalize(camera_buffer.pos.xyz - bb_center);
  float bb_size = length(bb.max.xyz - bb.min.xyz) / 2;
  vec3 closest_bb_point = bb_center + dir_to_cam * bb_size;

  vec4 projected = (camera_buffer.vp * vec4(closest_bb_point, 1));
  float new_depth = projected.z / projected.w;

  // BoundSphere bs = transform_bound_sphere(bound_sphere(mesh_data), transfrom);
  // vec3 dir_to_cam_from_bs = normalize(camera_buffer.pos.xyz - bs.center);
  // vec4 projected_center = camera_buffer.vp * vec4(bs.center + dir_to_cam_from_bs * bs.radius, 1.0);
  // float depth_sphere = projected_center.z / projected_center.w;
  // new_depth = depth_sphere;

  // --- Check visibility ---
  // When group of object is at far distance they have equal new and old depth
  float bias = 0.00000;
  // If new_depth >= 1 it means that object is very big and clips with camera.
  // But we here after frustum culling so object is visible
  bool visible = new_depth >= 1 || new_depth < old_depth - bias;
#else // USE_BOUND_BOXES
	vec4 aabb;
  bool visible = true;
  BoundSphere bs = transform_bound_sphere(bound_sphere(mesh_data), transfrom);
  vec3 center_in_view = (camera_buffer.view * vec4(bs.center, 1)).xyz;
  float znear = camera_buffer.near;
  float p00 = camera_buffer.proj[0][0];
  float p11 = camera_buffer.proj[1][1];
	if (get_bound_sphere_screen_rectangle(center_in_view, bs.radius, znear, p00, p11, aabb)) {
		float width = (aabb.z - aabb.x) * buffers_data.depth_pyramid_width;
		float height = (aabb.w - aabb.y) * buffers_data.depth_pyramid_heigth;

		// Because we only consider 2x2 pixels, we need to make sure we are sampling from a mip that reduces the rectangle to 1x1 texel or smaller.
		// Due to the rectangle being arbitrarily offset, a 1x1 rectangle may cover 2x2 texel area. Using floor() here would require sampling 4 corners
		// of AABB (using bilinear fetch), which is a little slower.
		float level = ceil(log2(max(width, height)));

    vec2 mip_scale = vec2(depth_mip_scales[2 * uint(level)], depth_mip_scales[2 * uint(level) + 1]);
    vec2 tex_coord = ((aabb.xy + aabb.zw) * 0.5) * mip_scale;

		// Sampler is set up to do max reduction, so this computes the max depth of a 2x2 texel quad
		float depth = textureLod(DepthPyramid, tex_coord, level).x;
    // TODO(dk6): try write it without matrix multiplication
    vec3 dir_to_cam = normalize(camera_buffer.pos.xyz - bs.center);
    vec4 projected_center = camera_buffer.vp * vec4(bs.center + dir_to_cam * bs.radius, 1.0);
    float depth_sphere = projected_center.z / projected_center.w;

		visible = depth_sphere <= depth;
	}
#endif // USE_BOUND_BOXES

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
