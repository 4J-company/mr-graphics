/**/
#version 460 // required for gl_DrawID (https://wikis.khronos.org/opengl/Vertex_Shader/Defined_Inputs)

// For uniforms array
#extension GL_EXT_nonuniform_qualifier : enable

#ifndef BINDLESS_SET
#define BINDLESS_SET 0
#endif

#include "types.h"

layout(location = 0) in vec3 InPos;

layout(push_constant) uniform DrawsIndosBufferId {
  uint draw_infos_buffer;
  uint camera_buffer_id;
};

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DrawIndoBuffers {
  MeshDrawInfo draws[];
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
#define transforms SSBOArray[draw.transforms_buffer_id].transforms

void main()
{
  mat4 transform = transpose(transforms[gl_InstanceIndex]);
  gl_Position = cam_ubo.vp * position;
  gl_Position = vec4(gl_Position.x, -gl_Position.y, gl_Position.z, gl_Position.w);
}
