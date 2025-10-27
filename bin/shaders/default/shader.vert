/**/
#version 460 // required for gl_DrawID (https://wikis.khronos.org/opengl/Vertex_Shader/Defined_Inputs)

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec4 InColor;
layout(location = 2) in vec3 InNorm;
layout(location = 3) in vec3 InTan;
layout(location = 4) in vec3 InBiTan;
layout(location = 5) in vec2 InTexCoord;

layout(location = 0) out vec4 position;
layout(location = 1) out vec4 normal;
layout(location = 2) out vec4 color;
layout(location = 3) out vec4 metallic_roughness;
layout(location = 4) out vec4 emissive;
layout(location = 5) out vec4 occlusion;

#include "pbr_params.h"

struct DrawInfo {
  uint mesh_offset;
  uint instance_offset;
  uint material_buffer_id;
  uint camera_buffer_id;
  uint transforms_buffer_id;
};

layout(push_constant) uniform DrawsIndosBufferId {
  uint draw_infos_buffer;
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
#define cam_ubo CameraUboArray[draw.camera_buffer_id]

layout(set = BINDLESS_SET, binding = STORAGE_BUFFERS_BINDING) readonly buffer Transforms {
  mat4 transforms[];
} SSBOArray[];
#define transforms SSBOArray[draw.transforms_buffer_id].transforms

void main()
{
  // TODO(dk6): move readings from texture to fragment shader, because these readings can be useless if fragment isn't on screen
  vec2 tex_coord = InTexCoord.xy;
  uint mat_id = draw.material_buffer_id;
  vec4 base_color = get_base_color(mat_id, tex_coord);
  vec4 metallic_roughness_color = get_metallic_roughness_color(mat_id, tex_coord);
  vec4 emissive_color = get_emissive_color(mat_id, tex_coord);
  vec4 occlusion_color = get_occlusion_color(mat_id, tex_coord);
  vec4 normal_color = get_normal_color(mat_id, tex_coord);

  mat4 transform = transpose(transforms[draw.instance_offset + gl_InstanceIndex]);
  // mat4 transform = transpose(transforms[gl_DrawID + gl_InstanceIndex]);

  position = transform * vec4(InPos.xyz, 1.0);
  color = base_color;
  metallic_roughness = metallic_roughness_color;
  emissive = emissive_color;
  occlusion = occlusion_color;

  // TODO(dk6): added normal maps. You can see this:
  // OutNIsShade = vec4(normalize(mix(DrawNormal, mat3(DrawTangent, DrawBitangent, DrawNormal) *
  //               (texture(NormalMap, DrawTexCoord).rgb * 2 - vec3(1, 1, 1)), TexFlags1.y)), 1);
  normal = vec4(InNorm, 0);

  gl_Position = cam_ubo.vp * transform * vec4(InPos.xyz, 1.0);
  gl_Position = vec4(gl_Position.x, -gl_Position.y, gl_Position.z, gl_Position.w);
}
