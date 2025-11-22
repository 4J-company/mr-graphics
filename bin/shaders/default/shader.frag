/**/
#version 450
layout(location = 0) out vec4 OutPos;
layout(location = 1) out vec4 OutNIsShade;
layout(location = 2) out vec4 OutMR;
layout(location = 3) out vec4 OutEmissive;
layout(location = 4) out vec4 OutOcclusion;
layout(location = 5) out vec4 OutColorTrans;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in flat uint materialid;

#include "pbr_params.h"

void main()
{
  vec4 bckg_color = vec4(0.3, 0.47, 0.8, 1);

  OutPos = position;
  OutNIsShade = normal;

  OutMR         = get_metallic_roughness_color(materialid, texcoord);
  OutEmissive   = get_emissive_color(materialid, texcoord);
  OutOcclusion  = get_occlusion_color(materialid, texcoord);
  OutColorTrans = get_base_color(materialid, texcoord);
}
