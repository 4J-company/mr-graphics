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

#define PBR_SET_INDEX 0

layout(set = PBR_SET_INDEX, binding = 1) uniform PrimitiveUbo {
  mat4 transform;

  vec4 base_color_factor;
  vec4 metallic_roughness_factor;
  vec4 emissive_factor;
  vec4 occlusion_factor;
  vec4 normal;
} ubo;

#if BASE_COLOR_MAP_PRESENT
layout(set = PBR_SET_INDEX, binding = 2) uniform sampler2D BaseColor;
#endif
#if METALLIC_ROUGHNESS_MAP_PRESENT
layout(set = PBR_SET_INDEX, binding = 3) uniform sampler2D MetallicRoughness;
#endif
#if EMISSIVE_MAP_PRESENT
layout(set = PBR_SET_INDEX, binding = 4) uniform sampler2D Emissive;
#endif
#if OCCLUSION_MAP_PRESENT
layout(set = PBR_SET_INDEX, binding = 5) uniform sampler2D Occlusion;
#endif
#if NORMAL_MAP_PRESENT
layout(set = PBR_SET_INDEX, binding = 6) uniform sampler2D Normal;
#endif

vec4 get_base_color(vec2 tex_coord) {
#if BASE_COLOR_MAP_PRESENT
  return texture(BaseColor, tex_coord);
#else
  return ubo.base_color_factor;
#endif
}

vec4 get_metallic_roughness_color(vec2 tex_coord) {
#if METALLIC_ROUGHNESS_MAP_PRESENT
  return texture(MetallicRoughness, tex_coord);
#else
  return ubo.metallic_roughness_factor;
#endif
}

vec4 get_emissive_color(vec2 tex_coord) {
#if EMISSIVE_MAP_PRESENT
  return texture(Emissive, tex_coord);
#else
  return ubo.emissive_factor;
#endif
}

vec4 get_occlusion_color(vec2 tex_coord) {
#if OCCLUSION_MAP_PRESENT
  return texture(Occlusion, tex_coord);
#else
  return ubo.occlusion_factor;
#endif
}

vec4 get_normal_color(vec2 tex_coord) {
#if NORMAL_MAP_PRESENT
  return texture(Normal, tex_coord);
#else
  return vec4(InNorm, 1.0);
#endif
}
