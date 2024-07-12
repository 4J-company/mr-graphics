/**/
#version 450

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec4 InColor;
layout(location = 2) in vec2 InTex;
layout(location = 3) in vec2 InNorm;
layout(location = 4) in vec3 InTan;
layout(location = 5) in vec3 InBitan;

layout(location = 0) out vec2 tex_coord_out;

layout(set = 0, binding = 1) uniform PrimitiveUbo
{
  mat4 vp;
} ubo;

void main()
{
  vec3 pos = InPos;
  gl_Position = ubo.vp * vec4(InPos, 1.0);
  tex_coord_out = InTex;
}
