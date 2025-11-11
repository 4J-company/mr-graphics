/**/
#version 460

// For uniforms array
#extension GL_EXT_nonuniform_qualifier : enable

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
  mat4 vp;
  vec4 pos;
  float fov;
  float gamma;
  float speed;
  float sens;
} CameraUboArray[];
#define cam_uniform_buffer CameraUboArray[camera_ubo_id]

layout(set = BINDLESS_SET, binding = UNIFORM_BUFFERS_BINDING) readonly uniform LightUbo {
  vec4 direction;
  vec4 color;
} DirectionLightsUboArray[];
#define light_uniform_buffer DirectionLightsUboArray[light_ubo_id]

#include "gamma_correction.h"
#include "tone_mapping.h"

#include "phong_logic.h"
#include "pbr_logic.h"

void main( void )
{
  vec4 pos = subpassLoad(InPos);
  vec3 color = subpassLoad(InColorTrans).xyz;
  vec3 norm = subpassLoad(InNIsShade).xyz;

  if (norm == vec3(0)) {
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
}


