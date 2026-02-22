/**/
#version 460 // required for gl_DrawID (https://wikis.khronos.org/opengl/Vertex_Shader/Defined_Inputs)

// For uniforms array
#extension GL_EXT_nonuniform_qualifier : enable

#ifndef BINDLESS_SET
#define BINDLESS_SET 0
#endif

#include "types.h"

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec4 InColor;
layout(location = 2) in vec3 InNorm;
layout(location = 3) in vec3 InTan;
layout(location = 4) in vec3 InBiTan;
layout(location = 5) in vec2 InTexCoord;

layout(location = 0) out vec4 position;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec2 texcoord;
layout(location = 3) out flat uint materialid;
layout(location = 4) out flat uint instance_id;

#ifdef ENABLE_CULLING_VISUALIZATION
layout(location = 5) out flat uint visible_at_stash;
#endif // ENABLE_CULLING_VISUALIZATION

layout(push_constant) uniform DrawsIndosBufferId {
  uint draw_infos_buffer;
  uint transforms_buffer_id;
  uint camera_buffer_id;
  uint visibility_states_buffer_id; // Used if ENABLE_CULLING_VISUALIZATION defined
};

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer DrawIndoBuffers {
  MeshDrawInfo draws[];
} DrawInfosArray[];
#define draws DrawInfosArray[draw_infos_buffer].draws
#define draw draws[gl_DrawID]

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform CameraUbo {
  CameraData data;
} CameraUboArray[];
#define cam_ubo CameraUboArray[camera_buffer_id].data

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer InstancesRenderInfosBuffer {
  InstanceDrawInfo infos[];
} InstancesRenderInfos[];
#define transform_index InstancesRenderInfos[draw.instance_render_info_buffer_id].infos[gl_InstanceIndex].transforms_index

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer Transforms {
  mat4 transforms[];
} TransfromsArray[];
#define transforms TransfromsArray[transforms_buffer_id].transforms

#ifdef ENABLE_CULLING_VISUALIZATION
layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) buffer VisibilityStatesBuffer {
  uint data[];
} VisibilityStates[];
#define visibility_states VisibilityStates[visibility_states_buffer_id].data
#endif // ENABLE_CULLING_VISUALIZATION

void main()
{
  texcoord = InTexCoord;
  materialid = draw.material_buffer_id;

  mat4 transform = transpose(transforms[transform_index]);
  position = transform * vec4(InPos.xyz, 1.0);
  normal = InNorm;
  instance_id = transform_index;

  gl_Position = cam_ubo.vp * position;
  gl_Position = vec4(gl_Position.x, -gl_Position.y, gl_Position.z, gl_Position.w);

#ifdef ENABLE_CULLING_VISUALIZATION
  visible_at_stash = visibility_states[transform_index];
#endif // ENABLE_CULLING_VISUALIZATION
}
