/**/
#version 460

layout(location = 0) out vec4 OutColor;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput InPos;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput InNIsShade;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput InOMR;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput InEmissive;
layout(input_attachment_index = 5, set = 0, binding = 5) uniform subpassInput InColorTrans;

layout(set = 0, binding = 6) uniform CameraUbo {
  mat4 vp;
  vec4 pos;
  float fov;
  float gamma;
  float speed;
  float sens;
} cam_ubo;

#include "gamma_correction.h"
#include "tone_mapping.h"

#include "phong_logic.h"
#include "pbr_logic.h"

void main( void )
{
  vec4 pos = subpassLoad(InPos);
  vec3 color = subpassLoad(InColorTrans).xyz;
  vec3 norm = subpassLoad(InNIsShade).xyz;

  vec3 occlusion_roughness_metallic = subpassLoad(InOMR).xyz;
  vec3 emissive = subpassLoad(InEmissive).xyz;

  float occlusion = occlusion_roughness_metallic.r;
  float roughness = occlusion_roughness_metallic.g;
  float metallic  = occlusion_roughness_metallic.b;

  vec3 poses[] = {vec3(-1, 1, 1), vec3(1, 1, -1)};
  vec3 result = vec3(0);
  for (int i = 0; i < 2; i++) {
    PointData pd = PointData(poses[i], vec3(1.0), pos.xyz, norm);

    vec3 shaded_color = ShadePBR(pd, color, emissive, occlusion, metallic, roughness);
    vec3 tonemapped_color = tm_aces(shaded_color);
    vec3 gamma_corrected_color = gc_linear(tonemapped_color);

    vec3 final_color = gamma_corrected_color;

    result += final_color;
  }
  OutColor = vec4(result, 0);
}


