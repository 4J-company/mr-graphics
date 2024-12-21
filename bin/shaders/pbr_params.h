#define PBR_SET_INDEX 0

layout(set = PBR_SET_INDEX, binding = 1) uniform PrimitiveUbo {
  mat4 transform;

  vec4 base_color_factor;
  vec4 metallic_roughness_factor;
  vec4 emissive_factor;
  vec4 occlusion_factor;
  vec4 normal;
} ubo;

#ifdef BASE_COLOR_MAP_BINDING
layout(set = PBR_SET_INDEX, binding = BASE_COLOR_MAP_BINDING) uniform sampler2D BaseColor;
#endif
#ifdef METALLIC_ROUGHNESS_MAP_BINDING
layout(set = PBR_SET_INDEX, binding = METALLIC_ROUGHNESS_MAP_BINDING) uniform sampler2D MetallicRoughness;
#endif
#ifdef EMISSIVE_MAP_BINDING
layout(set = PBR_SET_INDEX, binding = EMISSIVE_MAP_BINDING) uniform sampler2D Emissive;
#endif
#ifdef OCCLUSION_MAP_BINDING
layout(set = PBR_SET_INDEX, binding = OCCLUSION_MAP_BINDING) uniform sampler2D Occlusion;
#endif
#ifdef NORMAL_MAP_BINDING
layout(set = PBR_SET_INDEX, binding = NORMAL_MAP_BINDING) uniform sampler2D Normal;
#endif

vec4 get_base_color(vec2 tex_coord) {
#ifdef BASE_COLOR_MAP_BINDING
  return texture(BaseColor, tex_coord) * ubo.base_color_factor;
#else
  return ubo.base_color_factor;
#endif
}

vec4 get_metallic_roughness_color(vec2 tex_coord) {
#ifdef METALLIC_ROUGHNESS_MAP_BINDING
  return texture(MetallicRoughness, tex_coord) * ubo.metallic_roughness_factor;
#else
  return ubo.metallic_roughness_factor;
#endif
}

vec4 get_emissive_color(vec2 tex_coord) {
#ifdef EMISSIVE_MAP_BINDING
  return texture(Emissive, tex_coord) * ubo.emissive_factor
#else
  return ubo.emissive_factor;
#endif
}

vec4 get_occlusion_color(vec2 tex_coord) {
#ifdef OCCLUSION_MAP_BINDING
  return texture(Occlusion, tex_coord) * ubo.occlusion_factor;
#else
  return ubo.occlusion_factor;
#endif
}

vec4 get_normal_color(vec2 tex_coord) {
#ifdef NORMAL_MAP_BINDING
  return texture(Normal, tex_coord);
#else
  return vec4(InNorm, 1.0);
#endif
}
