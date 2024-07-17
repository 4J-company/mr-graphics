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
layout(location = 2) out vec3 normal;

layout(set = 0, binding = 1) uniform PrimitiveUbo
{
  mat4 vp;
} ubo;

vec3 Shade( vec3 LightDir, vec3 LightColor, vec3 P, vec3 N, vec3 Kd, vec3 Ks, float Ph )
{
  vec3 L = normalize(vec3(LightDir.x, -LightDir.y, LightDir.z));
  vec3 color = vec3(0, 0, 0);
  vec3 V = normalize(P - vec3(1, 1, 1));

  N = normalize(faceforward(N, V, N));

  color += max(0.1, dot(N, L)) * Kd * LightColor;

  vec3 R = reflect(V, N);
  color += pow(max(0.1, dot(R, L)), Ph) * Ks * LightColor;

  return color;
}

void main()
{
  vec4 pos = vec4(InPos * 10.0, 1.0);
  gl_Position = ubo.vp * pos;
  tex_coord_out = vec2(InTex);
  color = Shade(vec3(1, 1, 1), vec3(1, 1, 1), pos.xyz, InNorm, vec3(0.8), vec3(0.47), 0.30);
}
