/**/
#version 450

#ifndef BASE_COLOR_MAP_PRESENT
#define BASE_COLOR_MAP_PRESENT 0
#endif
#ifndef METALLIC_ROUGHNESS_MAP_PRESENT
#define METALLIC_ROUGHNESS_MAP_PRESENT 0
#endif
#ifndef EMISSIVE_MAP_PRESENT
#define EMISSIVE_MAP_PRESENT 0
#endif
#ifndef OCCLUSION_MAP_PRESENT
#define OCCLUSION_MAP_PRESENT 0
#endif
#ifndef NORMAL_MAP_PRESENT
#define NORMAL_MAP_PRESENT 0
#endif

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec3 InNorm;
layout(location = 2) in vec2 InTexCoord;

layout(location = 0) out vec4 position;
layout(location = 1) out vec4 normal;
layout(location = 2) out vec4 color;
layout(location = 3) out vec4 metallic_roughness;
layout(location = 4) out vec4 emissive;
layout(location = 5) out vec4 occlusion;

layout(set = 0, binding = 0) uniform PrimitiveUbo {
  mat4 transform;

  vec4 base_color_factor;
  vec4 metallic_roughness_factor;
  vec4 emissive_factor;
  vec4 occlusion_factor;
  vec4 normal;
} ubo;

#if BASE_COLOR_MAP_PRESENT
layout(set = 0, binding = 1) uniform sampler2D BaseColor;
#endif
#if METALLIC_ROUGHNESS_MAP_PRESENT
layout(set = 0, binding = 2) uniform sampler2D MetallicRoughness;
#endif
#if EMISSIVE_MAP_PRESENT
layout(set = 0, binding = 3) uniform sampler2D Emissive;
#endif
#if OCCLUSION_MAP_PRESENT
layout(set = 0, binding = 4) uniform sampler2D Occlusion;
#endif
#if NORMAL_MAP_PRESENT
layout(set = 0, binding = 5) uniform sampler2D Normal;
#endif

void main()
{
  vec2 tex_coord = InTexCoord.xy;

#if BASE_COLOR_MAP_PRESENT
  vec4 base_color = texture(BaseColor, tex_coord);
#else
  vec4 base_color = ubo.base_color_factor;
#endif

#if METALLIC_ROUGHNESS_MAP_PRESENT
  vec4 metallic_roughness_color = texture(MetallicRoughness, tex_coord);
#else
  vec4 metallic_roughness_color = ubo.metallic_roughness_factor;
#endif

#if EMISSIVE_MAP_PRESENT
  vec4 emissive_color = texture(Emissive, tex_coord);
#else
  vec4 emissive_color = ubo.emissive_factor;
#endif

#if OCCLUSION_MAP_PRESENT
  vec4 occlusion_color = texture(Occlusion, tex_coord);
#else
  vec4 occlusion_color = ubo.occlusion_factor;
#endif

#if NORMAL_MAP_PRESENT
  vec4 normal_color = texture(Normal, tex_coord);
#else
  vec4 normal_color = vec4(InNorm, 1.0);
#endif

  position = vec4(InPos.x, -InPos.y, InPos.z, 1.0);
  normal = normal_color;
  color = base_color;
  metallic_roughness = metallic_roughness_color;
  emissive = emissive_color;
  occlusion = occlusion_color;

  gl_Position = ubo.transform * position;
}
