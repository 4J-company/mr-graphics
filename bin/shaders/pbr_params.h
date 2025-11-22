#ifndef BINDLESS_SET
#define BINDLESS_SET 0
#endif

// For texture array
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform PrimitiveUbo {
  vec4 base_color_factor;
  vec4 emissive_color;
  float emissive_strength;
  float normal_map_intensity;
  float roughness_factor;
  float metallic_factor;

  uint base_color_tex_id;
  uint metallic_roughness_tex_id;
  uint emissive_tex_id;
  uint occlusion_tex_id;
  uint normal_map_tex_id;
} UniformBuffersArray[];

layout(set = BINDLESS_SET, binding = TEXTURES_BINDING) uniform sampler2D TexturesArray[];

#define ubo(mat_id) UniformBuffersArray[mat_id]

#define BaseColorTex(mat_id) TexturesArray[ubo(mat_id).base_color_tex_id]
#define MetallicRoughnessTex(mat_id) TexturesArray[ubo(mat_id).metallic_roughness_tex_id]
#define EmissiveTex(mat_id) TexturesArray[ubo(mat_id).emissive_tex_id]
#define OcclusionTex(mat_id) TexturesArray[ubo(mat_id).occlusion_tex_id]
#define NormalMapTex(mat_id) TexturesArray[ubo(mat_id).normal_map_tex_id]

vec4 get_base_color(uint mat_id, vec2 tex_coord) {
#ifdef BASE_COLOR_MAP_BINDING
  return texture(BaseColorTex(mat_id), tex_coord) * ubo(mat_id).base_color_factor;
#else
  return ubo(mat_id).base_color_factor;
#endif
}

vec4 get_metallic_roughness_color(uint mat_id, vec2 tex_coord) {
#ifdef METALLIC_ROUGHNESS_MAP_BINDING
  return texture(MetallicRoughnessTex(mat_id), tex_coord) * vec4(1.f, ubo(mat_id).metallic_factor, ubo(mat_id).roughness_factor, 1);
#else
  return vec4(1.f, ubo(mat_id).metallic_factor, ubo(mat_id).roughness_factor, 1);
#endif
}

vec4 get_emissive_color(uint mat_id, vec2 tex_coord) {
#ifdef EMISSIVE_MAP_BINDING
  return texture(EmissiveTex(mat_id), tex_coord) * ubo(mat_id).emissive_strength;
#else
  return ubo(mat_id).emissive_color * ubo(mat_id).emissive_strength;
#endif
}

vec4 get_occlusion_color(uint mat_id, vec2 tex_coord) {
#ifdef OCCLUSION_MAP_BINDING
  return texture(OcclusionTex(mat_id), tex_coord);
#else
  return vec4(0.f);
#endif
}

vec4 get_normal_color(uint mat_id, vec2 tex_coord) {
#ifdef NORMAL_MAP_BINDING
  // TODO(dk6): added normal maps. Reference implementation
  // OutNIsShade = vec4(normalize(mix(DrawNormal, mat3(DrawTangent, DrawBitangent, DrawNormal) *
  //               (texture(NormalMap, DrawTexCoord).rgb * 2 - vec3(1, 1, 1)), TexFlags1.y)), 1);
  // return texture(NormalMapTex(mat_id), tex_coord) * ubo(mat_id).normal_map_intensity;
  return normal;
#else
  return normal;
#endif
}
