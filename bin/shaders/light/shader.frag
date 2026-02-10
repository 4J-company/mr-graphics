/**/
#version 460

// For uniforms array
#extension GL_EXT_nonuniform_qualifier : enable

#include "types.h"

layout(location = 0) out vec4 OutColor;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput InPos;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput InNIsShade;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput InOMR;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput InEmissive;
layout(input_attachment_index = 5, set = 0, binding = 5) uniform subpassInput InColorTrans;

layout(push_constant) uniform Offsets {
  uint camera_ubo_id;
  uint light_ubo_id;
};

#define BINDLESS_SET 1

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform CameraUbo {
  CameraData data;
} CameraUboArray[];
#define cam_uniform_buffer CameraUboArray[camera_ubo_id].data

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform LightUbo {
  vec4 direction;
  vec4 color;
} DirectionLightsUboArray[];
#define light_uniform_buffer DirectionLightsUboArray[light_ubo_id]

#include "gamma_correction.h"
#include "tone_mapping.h"

#include "phong_logic.h"
#include "pbr_logic.h"

// TODO(dk6): move to utils.h
uint hash_u32(uint x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

vec3 hash_rgb(uint id) {
  uint h = hash_u32(id);
  return vec3(
    float((h >>  0) & 255u),
    float((h >>  8) & 255u),
    float((h >> 16) & 255u)
  ) / 255.0;
}

void main( void )
{
  vec4 pos = subpassLoad(InPos);
  vec3 color = subpassLoad(InColorTrans).xyz;
  vec4 norm_is_shade = subpassLoad(InNIsShade);
  vec3 norm = -norm_is_shade.xyz;
  bool is_shade = norm_is_shade.w != 0;

  if (!is_shade) {
    OutColor = vec4(color, 1);
    return;
  }

  vec3 occlusion_roughness_metallic = subpassLoad(InOMR).xyz;
  vec3 emissive = subpassLoad(InEmissive).xyz;

  float occlusion = occlusion_roughness_metallic.r;
  // TODO(dk6): rename texture
  float metallic  = occlusion_roughness_metallic.g;
  float roughness = occlusion_roughness_metallic.b;

  vec3 light_dir = light_uniform_buffer.direction.xyz;
  vec3 light_color = light_uniform_buffer.color.xyz;

  PointData pd = PointData(light_dir, light_color, pos.xyz, norm);

  vec3 shaded_color = ShadePBR(pd, color, emissive, occlusion, metallic, roughness);
  vec3 tonemapped_color = tm_aces(shaded_color);
  vec3 gamma_corrected_color = gc_linear(tonemapped_color);

  vec3 final_color = gamma_corrected_color;
  OutColor = vec4(final_color, 1);

#ifdef HASH_COLORING
  uint id = uint(floatBitsToUint(pos.w));
  OutColor = vec4(hash_rgb(id), 1);
  return;
#endif // HASH_COLORING
}


