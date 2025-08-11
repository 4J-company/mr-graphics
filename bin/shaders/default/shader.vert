/**/
#version 450

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec3 InNorm;
layout(location = 2) in vec2 InTexCoord;

layout(location = 0) out vec4 position;
layout(location = 1) out vec4 normal;
layout(location = 2) out vec4 color;
layout(location = 3) out vec4 metallic_roughness;
layout(location = 4) out vec4 emissive;
layout(location = 5) out vec4 occlusion;

layout(set = 0, binding = 0) uniform CameraUbo {
  mat4 vp;
  vec4 pos;
  float fov;
  float gamma;
  float speed;
  float sens;
} cam_ubo;

#include "pbr_params.h"

void main()
{
  vec2 tex_coord = InTexCoord.xy;
  vec4 base_color = get_base_color(tex_coord);
  vec4 metallic_roughness_color = get_metallic_roughness_color(tex_coord);
  vec4 emissive_color = get_emissive_color(tex_coord);
  vec4 occlusion_color = get_occlusion_color(tex_coord);
  vec4 normal_color = get_normal_color(tex_coord);

  position = ubo.transform * vec4(InPos.xyz, 1.0);
  color = base_color;
  metallic_roughness = metallic_roughness_color;
  emissive = emissive_color;
  occlusion = occlusion_color;

  // TODO(dk6): added normal maps. You can see this:
  // OutNIsShade = vec4(normalize(mix(DrawNormal, mat3(DrawTangent, DrawBitangent, DrawNormal) *
  //               (texture(NormalMap, DrawTexCoord).rgb * 2 - vec3(1, 1, 1)), TexFlags1.y)), 1);
  normal = vec4(InNorm, 0);

  gl_Position = cam_ubo.vp * ubo.transform * vec4(InPos.xyz, 1.0);
  gl_Position = vec4(gl_Position.x, -gl_Position.y, gl_Position.z, gl_Position.w);
}
