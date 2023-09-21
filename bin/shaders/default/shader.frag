/**/
#version 450
layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 tex_coord;
layout(binding = 0) uniform sampler2D tex;

void main() 
{
  vec4 bckg_color = vec4(0.3, 0.47, 0.8, 1);
  vec4 tex_color = texture(tex, tex_coord);
  out_color = vec4(tex_color.rgb * tex_color.a + bckg_color.rgb * (1 - tex_color.a), 1);
}