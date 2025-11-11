#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout (points) in;
layout (line_strip, max_vertices = 24) out;

layout(location = 0) in flat uint instance_indexes[];
#define instance_index instance_indexes[0]

layout(push_constant) uniform PushContants {
  uint camera_buffer_id;
  uint bound_boxes_data;
} draw;

struct DrawData {
  uint transforms_buffer_id;
  uint transform_index;
  uint bound_boxes_buffer_id;
  uint bound_box_index;
};

struct BoundBox {
  vec4 minBB;
  vec4 maxBB;
};

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DrawCommandsBuffer {
  DrawData[] data;
} DrawCommands[];
#define bb_data DrawCommands[draw.bound_boxes_data].data[instance_index]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer BoudBoxesBuffer {
  BoundBox[] data;
} BoundBoxes[];
#define bb BoundBoxes[bb_data.bound_boxes_buffer_id].data[bb_data.bound_box_index]

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

void main()
{
  // bounding box vertexes
  vec3 v[8];
  v[0] = vec3(bb.minBB.x, bb.minBB.y, bb.minBB.z);
  v[1] = vec3(bb.maxBB.x, bb.minBB.y, bb.minBB.z);
  v[2] = vec3(bb.maxBB.x, bb.maxBB.y, bb.minBB.z);
  v[3] = vec3(bb.minBB.x, bb.maxBB.y, bb.minBB.z);
  v[4] = vec3(bb.minBB.x, bb.minBB.y, bb.maxBB.z);
  v[5] = vec3(bb.maxBB.x, bb.minBB.y, bb.maxBB.z);
  v[6] = vec3(bb.maxBB.x, bb.maxBB.y, bb.maxBB.z);
  v[7] = vec3(bb.minBB.x, bb.maxBB.y, bb.maxBB.z);

  mat4 mvp = cam_ubo.vp * transpose(transform);

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
