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
layout(location = 2) in vec4 color;
layout(location = 3) in vec4 metallic_roughness;
layout(location = 4) in vec4 emissive;
layout(location = 5) in vec4 occlusion;

void main()
{
  vec4 bckg_color = vec4(0.3, 0.47, 0.8, 1);

  OutPos = position;
  OutNIsShade = normal;
  OutMR = metallic_roughness;
  OutEmissive = emissive;
  OutOcclusion = occlusion;
  OutColorTrans = color;
}
