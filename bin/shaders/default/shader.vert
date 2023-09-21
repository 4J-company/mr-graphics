/**/
#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 tex_coord;

layout(location = 0) out vec2 tex_coord_out;

void main() 
{
  gl_Position = vec4(position, 0.0, 1.0);
  tex_coord_out = tex_coord;
}