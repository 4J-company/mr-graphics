#define PBR_SET_INDEX 0

layout(set = PBR_SET_INDEX, binding = 1) /**
 * @brief Uniform block for physically based rendering (PBR) material parameters.
 *
 * This block encapsulates key material properties used in PBR shading, including a
 * transformation matrix and various factor vectors. These factors serve as defaults or
 * multipliers for texture values:
 * - **transform**: Matrix to transform object-space coordinates.
 * - **base_color_factor**: Base color used when no texture is sampled.
 * - **metallic_roughness_factor**: Combined metallic and roughness values.
 * - **emissive_factor**: Emissive color factor for self-illumination.
 * - **occlusion_factor**: Factor controlling ambient occlusion.
 * - **normal**: Default normal vector.
 */
uniform PrimitiveUbo {
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

/**
 * @brief Computes the material's base color.
 *
 * If a base color texture is available (indicated by BASE_COLOR_MAP_BINDING), the function multiplies the texture color sampled at the provided coordinates by the base color factor. Otherwise, it returns the base color factor directly.
 *
 * @param tex_coord The texture coordinates for sampling the base color texture.
 * @return The resulting base color as a vec4.
 */
vec4 get_base_color(vec2 tex_coord) {
#ifdef BASE_COLOR_MAP_BINDING
  return texture(BaseColor, tex_coord) * ubo.base_color_factor;
#else
  return ubo.base_color_factor;
#endif
}

/**
 * @brief Computes the metallic-roughness color for a PBR material.
 *
 * This function returns the combined metallic-roughness color by sampling the MetallicRoughness texture
 * using the given texture coordinates and multiplying it by the corresponding factor from the UBO.
 * If the metallic-roughness texture is not available, it returns the UBO's specified metallic-roughness factor directly.
 *
 * @param tex_coord Texture coordinates for sampling the MetallicRoughness texture.
 * @return vec4 The computed metallic-roughness color.
 */
vec4 get_metallic_roughness_color(vec2 tex_coord) {
#ifdef METALLIC_ROUGHNESS_MAP_BINDING
  return texture(MetallicRoughness, tex_coord) * ubo.metallic_roughness_factor;
#else
  return ubo.metallic_roughness_factor;
#endif
}

/**
 * @brief Retrieves the emissive color.
 *
 * If the emissive texture is defined (via EMISSIVE_MAP_BINDING), the function samples the emissive texture
 * at the provided texture coordinate and multiplies it by the emissive factor from the uniform block.
 * Otherwise, it returns the emissive factor directly.
 *
 * @param tex_coord Texture coordinate for sampling the emissive texture.
 * @return vec4 The computed emissive color.
 */
vec4 get_emissive_color(vec2 tex_coord) {
#ifdef EMISSIVE_MAP_BINDING
  return texture(Emissive, tex_coord) * ubo.emissive_factor
#else
  return ubo.emissive_factor;
#endif
}

/**
 * @brief Retrieves the occlusion color.
 *
 * If the occlusion texture is bound, samples the occlusion texture using the provided texture coordinate
 * and modulates the result by the occlusion factor from the UBO. Otherwise, returns the occlusion factor directly.
 *
 * @param tex_coord The texture coordinate used for sampling the occlusion texture.
 * @return vec4 The computed occlusion color.
 */
vec4 get_occlusion_color(vec2 tex_coord) {
#ifdef OCCLUSION_MAP_BINDING
  return texture(Occlusion, tex_coord) * ubo.occlusion_factor;
#else
  return ubo.occlusion_factor;
#endif
}

/**
 * @brief Retrieves the normal color for the given texture coordinates.
 *
 * This function returns the sampled normal color from the bound normal map if the NORMAL_MAP_BINDING directive is defined.
 * Otherwise, it returns a default normal vector with an alpha component of 1.0.
 *
 * @param tex_coord Texture coordinates used to sample the normal map.
 * @return vec4 The resulting normal color.
 */
vec4 get_normal_color(vec2 tex_coord) {
#ifdef NORMAL_MAP_BINDING
  return texture(Normal, tex_coord);
#else
  return vec4(InNorm, 1.0);
#endif
}
