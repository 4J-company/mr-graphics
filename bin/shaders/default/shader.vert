/**/
#version 460 // required for gl_DrawID (https://wikis.khronos.org/opengl/Vertex_Shader/Defined_Inputs)

// For uniforms array
#extension GL_EXT_nonuniform_qualifier : enable

#ifndef BINDLESS_SET
#define BINDLESS_SET 0
#endif

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec4 InColor;
layout(location = 2) in vec3 InNorm;
layout(location = 3) in vec3 InTan;
layout(location = 4) in vec3 InBiTan;
layout(location = 5) in vec2 InTexCoord;

layout(location = 0) out vec4 position;
layout(location = 1) out vec4 normal;
layout(location = 2) out vec2 texcoord;
layout(location = 3) out flat uint materialid;

struct DrawInfo {
  uint mesh_offset;
  uint instance_offset;
  uint material_buffer_id;
};

layout(push_constant) uniform DrawsIndosBufferId {
  uint draw_infos_buffer;
  uint camera_buffer_id;
  uint transforms_buffer_id;
};

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DrawIndoBuffers {
  DrawInfo draws[];
} DrawInfosArray[];
#define draws DrawInfosArray[draw_infos_buffer].draws
#define draw draws[gl_DrawID]

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform CameraUbo {
  mat4 vp;
  vec4 pos;
  float fov;
  float gamma;
  float speed;
  float sens;
} CameraUboArray[];
#define cam_ubo CameraUboArray[camera_buffer_id]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer Transforms {
  mat4 transforms[];
} SSBOArray[];
#define transforms SSBOArray[transforms_buffer_id].transforms

void main()
{
  uint instance_index = draw.instance_offset + gl_InstanceIndex;

  texcoord = InTexCoord;
  materialid = draw.material_buffer_id;

  mat4 transform = transpose(transforms[instance_index]);
  position = transform * vec4(InPos.xyz, 1.0);
  normal = vec4(InNorm, 1);

  gl_Position = cam_ubo.vp * position;
  gl_Position = vec4(gl_Position.x, -gl_Position.y, gl_Position.z, gl_Position.w);
}
