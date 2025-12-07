#version 460
#extension GL_EXT_nonuniform_qualifier : enable

#include "bounds.h"

layout (points) in;
layout (line_strip, max_vertices = 24) out;

layout(location = 0) in flat uint instance_indexes[];
#define instance_index instance_indexes[0]

layout(push_constant) uniform PushContants {
  uint camera_buffer_id;
  uint bound_boxes_data;
  uint render_bound_rects;
} draw;

struct DrawData {
  uint transforms_buffer_id;
  uint transform_index;
  uint bound_boxes_buffer_id;
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

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer Transforms {
  mat4 transforms[];
} TransformsArray[];
#define transform TransformsArray[bb_data.transforms_buffer_id].transforms[bb_data.transform_index]

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform CameraUbo {
  mat4 vp;
  vec4 pos;
  float fov;
  float gamma;
  float speed;
  float sens;
} CameraUboArray[];
#define cam_ubo CameraUboArray[draw.camera_buffer_id]

void render_bound_rectangle(BoundBox bb, mat4 mvp)
{
  vec4 rectangle = get_bound_box_screen_rectangle(bb, mvp);
  rectangle.y = -rectangle.y;
  rectangle.w = -rectangle.w;

  vec2 A = rectangle.xy, B = rectangle.zw;

  vec2 bottom_left = vec2(min(A.x, B.x), min(A.y, B.y));
  vec2 bottom_right = vec2(max(A.x, B.x), min(A.y, B.y));
  vec2 top_left = vec2(min(A.x, B.x), max(A.y, B.y));
  vec2 top_right = vec2(max(A.x, B.x), max(A.y, B.y));

  gl_Position = vec4(bottom_left, 0.0, 1.0);
  EmitVertex();
  gl_Position = vec4(bottom_right, 0.0, 1.0);
  EmitVertex();
  gl_Position = vec4(top_right, 0.0, 1.0);
  EmitVertex();
  gl_Position = vec4(top_left, 0.0, 1.0);
  EmitVertex();
  gl_Position = vec4(bottom_left, 0.0, 1.0);
  EmitVertex();
  EndPrimitive();

  // diagonals
  bool render_diagonals = false;
  if (render_diagonals) {
    gl_Position = vec4(bottom_left, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(top_right, 0.0, 1.0);
    EmitVertex();
    EndPrimitive();

    gl_Position = vec4(bottom_right, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(top_left, 0.0, 1.0);
    EmitVertex();
    EndPrimitive();
  }
}

void main()
{
  mat4 mvp = cam_ubo.vp * transpose(transform);
  BoundBox bb = bound_box;

  if (bool(draw.render_bound_rects)) {
    render_bound_rectangle(bb, mvp);
    return;
  }

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
  gl_Position = mvp * vec4(v[0], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[1], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();

  gl_Position = mvp * vec4(v[1], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[2], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();

  gl_Position = mvp * vec4(v[2], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[3], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();

  gl_Position = mvp * vec4(v[3], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[0], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();

  // Top (4 edges)
  gl_Position = mvp * vec4(v[4], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[5], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();

  gl_Position = mvp * vec4(v[5], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[6], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();

  gl_Position = mvp * vec4(v[6], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[7], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();

  gl_Position = mvp * vec4(v[7], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[4], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();

  // Вертикальные рёбра (4 ребра)
  gl_Position = mvp * vec4(v[0], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[4], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();

  gl_Position = mvp * vec4(v[1], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[5], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();

  gl_Position = mvp * vec4(v[2], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[6], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();

  gl_Position = mvp * vec4(v[3], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  gl_Position = mvp * vec4(v[7], 1.0);
  gl_Position.y *= -1;
  EmitVertex();
  EndPrimitive();
}
