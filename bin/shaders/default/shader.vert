/**/
#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 tex_coord;

layout(location = 0) out vec2 tex_coord_out;

layout(binding = 1) uniform PrimitiveUbo
{
  mat4 vp;
} ubo;

void main() 
{
  vec3 pos = vec3(position.x, 0, position.y) * 10;
  gl_Position = ubo.vp * vec4(pos, 1.0);
  // gl_Position = transpose(ubo.vp) * vec4(pos, 1.0);
  tex_coord_out = tex_coord;
}