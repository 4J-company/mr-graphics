#define PBR_SET_INDEX 0

layout(set = PBR_SET_INDEX, binding = 1) uniform PrimitiveUbo {
  mat4 transform;

  vec4 base_color_factor;
  vec4 emissive_color;
  float emissive_strength;
  float normal_map_intensity;
  float roughness_factor;
  float metallic_factor;
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
  return texture(MetallicRoughness, tex_coord) * vec4(1.f, ubo.metallic_factor, ubo.roughness_factor, 1);
#else
  return vec4(1.f, ubo.metallic_factor, ubo.roughness_factor, 1);
#endif
}

vec4 get_emissive_color(vec2 tex_coord) {
#ifdef EMISSIVE_MAP_BINDING
  return texture(Emissive, tex_coord) * ubo.emissive_strength;
#else
  return ubo.emissive_color * ubo.emissive_strength;
#endif
}

vec4 get_occlusion_color(vec2 tex_coord) {
#ifdef OCCLUSION_MAP_BINDING
  return texture(Occlusion, tex_coord);
#else
  return vec4(0.f);
#endif
}

vec4 get_normal_color(vec2 tex_coord) {
#ifdef NORMAL_MAP_BINDING
  return texture(Normal, tex_coord) * ubo.normal_map_intensity;
#else
  return vec4(InNorm, 1.0);
#endif
}
