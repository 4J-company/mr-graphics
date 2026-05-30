#version 460
#extension GL_EXT_nonuniform_qualifier : enable

#include "bounds.h"
#include "types.h"

layout (points) in;
// for box enough 24
// layout (line_strip, max_vertices = 24) out;
layout (line_strip, max_vertices = 160) out;

layout(location = 0) in flat uint instance_indexes[];
#define instance_index instance_indexes[0]

#ifdef ENABLE_BOUNDS_FRAG_COLOR
// We used packed color (in format 0xFF000000) because we can not use more than 128 * sizeof(vec4) bytes,
// where 128 is a value of 	maxGeometryOutputComponents (checked for RTX 5070)
layout(location = 1) out flat uint vertex_color;
#endif // ENABLE_BOUNDS_FRAG_COLOR

#ifndef STATE_DISABLE
#define STATE_DISABLE                  0
#endif // STATE_DISABLE

#ifndef STATE_BOUND_BOXES
#define STATE_BOUND_BOXES              1
#endif // STATE_BOUND_BOXES

#ifndef STATE_BOUND_BOX_RECTANGLES
#define STATE_BOUND_BOX_RECTANGLES     2
#endif // STATE_BOUND_BOX_RECTANGLES

#ifndef STATE_BOUND_SPHERES
#define STATE_BOUND_SPHERES            3
#endif // STATE_BOUND_SPHERES

#ifndef STATE_BOUND_SPHERE_RECTANGLES
#define STATE_BOUND_SPHERE_RECTANGLES  4
#endif // STATE_BOUND_SPHERE_RECTANGLES

#ifndef STATE_DYNAMIC_BEST
#define STATE_DYNAMIC_BEST             5
#endif // STATE_DYNAMIC_BEST

#ifndef STATE_DYNAMIC_BEST_RECTANGLES
#define STATE_DYNAMIC_BEST_RECTANGLES  6
#endif // STATE_DYNAMIC_BEST_RECTANGLES

layout(push_constant) uniform PushContants {
  uint camera_buffer_id;
  uint bound_boxes_data;
  uint render_bounds_state;
} draw;

struct DrawData {
  uint transforms_buffer_id;
  uint transform_index;
  uint bound_boxes_buffer_id;
  uint bound_spheres_buffer_id;
  uint bound_box_index;
};

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DrawCommandsBuffer {
  DrawData[] data;
} DrawCommands[];
#define bb_data DrawCommands[draw.bound_boxes_data].data[instance_index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer BoudBoxesBuffer {
  BoundBox[] data;
} BoundBoxes[];
#define bound_box BoundBoxes[bb_data.bound_boxes_buffer_id].data[bb_data.bound_box_index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer BoudSpheresBuffer {
  BoundSphere[] data;
} BoundSpheres[];
#define bound_sphere BoundSpheres[bb_data.bound_spheres_buffer_id].data[bb_data.bound_box_index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer Transforms {
  mat4 transforms[];
} TransformsArray[];
#define transform TransformsArray[bb_data.transforms_buffer_id].transforms[bb_data.transform_index]

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform CameraUbo {
  CameraData data;
} CameraUboArray[];
#define cam_ubo CameraUboArray[draw.camera_buffer_id].data

void set_frag_color(uint color)
{
#ifdef ENABLE_BOUNDS_FRAG_COLOR
  vertex_color = color;
#endif // ENABLE_BOUNDS_FRAG_COLOR
}

void render_bound_rectangle(vec4 rectangle, uint color)
{
  vec2 A = rectangle.xy, B = rectangle.zw;

  vec2 bottom_left = vec2(min(A.x, B.x), min(A.y, B.y));
  vec2 bottom_right = vec2(max(A.x, B.x), min(A.y, B.y));
  vec2 top_left = vec2(min(A.x, B.x), max(A.y, B.y));
  vec2 top_right = vec2(max(A.x, B.x), max(A.y, B.y));

  gl_Position = vec4(bottom_left, 0.0, 1.0);
  set_frag_color(color);
  EmitVertex();
  gl_Position = vec4(bottom_right, 0.0, 1.0);
  set_frag_color(color);
  EmitVertex();
  gl_Position = vec4(top_right, 0.0, 1.0);
  set_frag_color(color);
  EmitVertex();
  gl_Position = vec4(top_left, 0.0, 1.0);
  set_frag_color(color);
  EmitVertex();
  gl_Position = vec4(bottom_left, 0.0, 1.0);
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  // diagonals
  bool render_diagonals = false;
  if (render_diagonals) {
    gl_Position = vec4(bottom_left, 0.0, 1.0);
    set_frag_color(color);
    EmitVertex();
    gl_Position = vec4(top_right, 0.0, 1.0);
    set_frag_color(color);
    EmitVertex();
    EndPrimitive();

    gl_Position = vec4(bottom_right, 0.0, 1.0);
    set_frag_color(color);
    EmitVertex();
    gl_Position = vec4(top_left, 0.0, 1.0);
    set_frag_color(color);
    EmitVertex();
    EndPrimitive();
  }
}

// Tex space [0, 1] -> Screen space [-1,1]
vec4 convert_tex_rect_to_screen_rect(vec4 rect) {
  return (rect - vec4(0.5)) * vec4(2);
}

void render_br_from_bb(BoundBox bb, mat4 proj)
{
  vec4 rectangle = get_bound_box_screen_rectangle(bb, proj);
  render_bound_rectangle(convert_tex_rect_to_screen_rect(rectangle), 0xFF000000);
}

void render_br_from_bs()
{
  mat4 proj = cam_ubo.vp;
  BoundSphere bs = transform_bound_sphere(bound_sphere, transpose(transform));
  bs.center = (cam_ubo.view * vec4(bs.center, 1)).xyz;
  proj = cam_ubo.proj;
  // bs = bound_sphere;
	vec4 aabb;
  float znear = cam_ubo.near;
  float p00 = proj[0][0];
  float p11 = proj[1][1];
  bool v = get_bound_sphere_screen_rectangle(bs.center, bs.radius, znear, p00, p11, aabb);
  if (v) {
    render_bound_rectangle(convert_tex_rect_to_screen_rect(aabb), 0x0000FF00);
  }
}

void render_bound_box(BoundBox bb, mat4 proj, uint color)
{
  // bounding box vertexes
  vec3 v[8];
  v[0] = vec3(bb.min.x, bb.min.y, bb.min.z);
  v[1] = vec3(bb.max.x, bb.min.y, bb.min.z);
  v[2] = vec3(bb.max.x, bb.max.y, bb.min.z);
  v[3] = vec3(bb.min.x, bb.max.y, bb.min.z);
  v[4] = vec3(bb.min.x, bb.min.y, bb.max.z);
  v[5] = vec3(bb.max.x, bb.min.y, bb.max.z);
  v[6] = vec3(bb.max.x, bb.max.y, bb.max.z);
  v[7] = vec3(bb.min.x, bb.max.y, bb.max.z);

  // Bottom (4 edges)
  gl_Position = proj * vec4(v[0], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[1], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  gl_Position = proj * vec4(v[1], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[2], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  gl_Position = proj * vec4(v[2], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[3], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  gl_Position = proj * vec4(v[3], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[0], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  // Top (4 edges)
  gl_Position = proj * vec4(v[4], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[5], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  gl_Position = proj * vec4(v[5], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[6], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  gl_Position = proj * vec4(v[6], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[7], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  gl_Position = proj * vec4(v[7], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[4], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  // Вертикальные рёбра (4 ребра)
  gl_Position = proj * vec4(v[0], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[4], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  gl_Position = proj * vec4(v[1], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[5], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  gl_Position = proj * vec4(v[2], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[6], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();

  gl_Position = proj * vec4(v[3], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  gl_Position = proj * vec4(v[7], 1.0);
  gl_Position.y *= -1;
  set_frag_color(color);
  EmitVertex();
  EndPrimitive();
}

#define PI 3.14159265359

void render_bound_sphere(uint color)
{
  mat4 proj = cam_ubo.vp;
  BoundSphere bs = transform_bound_sphere(bound_sphere, transpose(transform));

  vec3 center = bs.center;
  float radius = bs.radius;

  const int latSegments = 8;
  const int lonSegments = 10;

  mat4 viewProj = proj;

  for (int lon = 0; lon < lonSegments; lon++) {
    float phi = float(lon) * 2.0 * PI / float(lonSegments);

    for (int lat = 0; lat <= latSegments; lat++) {
      float theta = float(lat) * PI / float(latSegments);

      float x = sin(theta) * cos(phi);
      float y = cos(theta);
      float z = sin(theta) * sin(phi);

      vec3 pos = center + radius * vec3(x, y, z);
      gl_Position = viewProj * vec4(pos, 1.0);
      gl_Position.y *= -1;
      set_frag_color(color);
      EmitVertex();
    }
    EndPrimitive();
  }

  for (int lat = 1; lat < latSegments; lat++) {
    float theta = float(lat) * PI / float(latSegments);

    for (int lon = 0; lon <= lonSegments; lon++) {
      float phi = float(lon) * 2.0 * PI / float(lonSegments);

      float x = sin(theta) * cos(phi);
      float y = cos(theta);
      float z = sin(theta) * sin(phi);

      vec3 pos = center + radius * vec3(x, y, z);
      gl_Position = viewProj * vec4(pos, 1.0);
      gl_Position.y *= -1;
      set_frag_color(color);
      EmitVertex();
    }
    EndPrimitive();
  }
}

bool best_screen_rect(BoundBox bb, BoundSphere bs, out vec4 rectangle, out bool is_bs) {
  vec3 center = (cam_ubo.view * vec4(bs.center, 1)).xyz;
  float znear = cam_ubo.near;
  BoundSphere bs_transformed = {center, bs.radius};
  return get_best_screen_rectangle(bb, bs_transformed, znear, cam_ubo.proj, cam_ubo.vp, rectangle, is_bs);
}

void main()
{
  mat4 proj = cam_ubo.vp;
  BoundBox bb = transform_bound_box(bound_box, transpose(transform));
  BoundSphere bs = transform_bound_sphere(bound_sphere, transpose(transform));

  if (draw.render_bounds_state == STATE_BOUND_BOXES) {
    render_bound_box(bb, proj, 0xFF000000);
  } else if (draw.render_bounds_state == STATE_BOUND_BOX_RECTANGLES) {
    render_br_from_bb(bb, proj);
  } else if (draw.render_bounds_state == STATE_BOUND_SPHERES) {
    render_bound_sphere(0x0000FF00);
  } else if (draw.render_bounds_state == STATE_BOUND_SPHERE_RECTANGLES) {
    render_br_from_bs();
  } else if (draw.render_bounds_state == STATE_DYNAMIC_BEST) {
    vec4 rect;
    bool is_bs;
    if (!best_screen_rect(bb, bs, rect, is_bs)) {
      // render_bound_rectangle(vec4(-0.25, -0.25, 0.25, 0.25), 0x00FF0000); // for debug
      return;
    }

    if (is_bs) {
      render_bound_sphere(0x0000FF00);
    } else {
      render_bound_box(bb, proj, 0xFF000000);
    }
  } else if (draw.render_bounds_state == STATE_DYNAMIC_BEST_RECTANGLES) {
    vec4 rect;
    bool is_bs;
    if (!best_screen_rect(bb, bs, rect, is_bs)) {
      return;
    }
    render_bound_rectangle(convert_tex_rect_to_screen_rect(rect), is_bs ? 0x0000FF00 : 0xFF000000);
  }
}
