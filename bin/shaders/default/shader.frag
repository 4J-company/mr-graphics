/**/
#version 450
layout(location = 0) out vec4 OutPos;
layout(location = 1) out vec4 OutNIsShade;
layout(location = 2) out vec4 OutKa;
layout(location = 3) out vec4 OutKd;
layout(location = 4) out vec4 OutKsPh;
layout(location = 5) out vec4 OutColorTrans;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;

void main()
{
  vec4 bckg_color = vec4(0.3, 0.47, 0.8, 1);

  OutPos = vec4(position, 1.0);
  OutNIsShade = vec4(normal, 1.0);
  OutKa = vec4(color, 1.0);
  OutKd = vec4(color, 1.0);
  OutKsPh = vec4(color, 1.0);
  OutColorTrans = vec4(color, 1.0);
}
