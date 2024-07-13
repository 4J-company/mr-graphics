/**/
#version 450

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec4 InColor;
layout(location = 2) in vec3 InTex;
layout(location = 3) in vec3 InNorm;
layout(location = 4) in vec3 InTan;
layout(location = 5) in vec3 InBitan;

layout(location = 0) out vec2 tex_coord_out;
layout(location = 1) out vec3 color;

layout(set = 0, binding = 1) uniform PrimitiveUbo
{
  mat4 vp;
} ubo;

void main()
{
  vec4 pos = vec4(InPos * 10.0, 1.0);
  gl_Position = ubo.vp * pos;
  tex_coord_out = vec2(InTex);
  color = InPos;
}
