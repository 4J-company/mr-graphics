/**/
#version 460

layout(location = 0) out vec4 OutColor;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput InPos;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput InNIsShade;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput InMR;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput InEmissive;
layout(input_attachment_index = 4, set = 0, binding = 4) uniform subpassInput InOcclusion;
layout(input_attachment_index = 5, set = 0, binding = 5) uniform subpassInput InColorTrans;

#include "phong_logic.h"
#include "pbr_logic.h"

void main( void )
{
  vec4 pos = subpassLoad(InPos);
  vec4 color = subpassLoad(InColorTrans);
  vec4 norm = subpassLoad(InNIsShade);

  vec4 roughness_metallic = subpassLoad(InMR);
  vec4 emissive = subpassLoad(InEmissive);
  vec4 occlusion = subpassLoad(InOcclusion);

  float metallic = roughness_metallic.b;
  float roughness = roughness_metallic.g;

  PointData pd = PointData(vec3(1.0), vec3(0.8, -0.47, 1.0), pos.xyz, norm.xyz);

  OutColor = vec4(ShadePBR(pd, color, emissive, occlusion, metallic, roughness), 1.0);
}
