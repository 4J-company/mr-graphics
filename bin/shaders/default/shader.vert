/**/
#version 450

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec3 InNorm;

layout(location = 0) out vec3 position;
layout(location = 1) out vec3 color;
layout(location = 2) out vec3 normal;

layout(set = 0, binding = 0) uniform PrimitiveUbo
{
  mat4 vp;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  float shininess;
} ubo;

void main()
{
  position = InPos * 0.5 + vec3(0.0, 0.0, -5.0);
  color = ubo.ambient;
  normal = ubo.diffuse;

  gl_Position = ubo.vp * vec4(position, 1.0);
}
