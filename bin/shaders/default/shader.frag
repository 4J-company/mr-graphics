/**/
#version 450
layout(location = 0) out vec4 OutPos;
layout(location = 1) out vec4 OutNIsShade;
layout(location = 2) out vec4 OutKa;
layout(location = 3) out vec4 OutKd;
layout(location = 4) out vec4 OutKsPh;
layout(location = 5) out vec4 OutColorTrans;

layout(location = 0) in vec2 tex_coord;
layout(set = 1, binding = 0) uniform sampler2D tex;

void main() 
{
  vec4 bckg_color = vec4(0.3, 0.47, 0.8, 1);
  vec4 tex_color = texture(tex, tex_coord);
  OutKd = vec4(tex_color.rgb * tex_color.a + bckg_color.rgb * (1 - tex_color.a), 1);
}
