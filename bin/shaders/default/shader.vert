/**/
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex_coord;

layout(location = 0) out vec2 tex_coord_out;

layout(binding = 1) uniform PrimitiveUbo
{
  mat4 vp;
} ubo;

void main() 
{
  vec3 pos = position;
  gl_Position = ubo.vp * vec4(pos, 1.0);
  tex_coord_out = tex_coord;
}