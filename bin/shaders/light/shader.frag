/**/
#version 460

layout(location = 0) out vec4 OutColor;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput InPos;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput InNIsShade;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput InKa;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput InKd;
layout(input_attachment_index = 4, set = 0, binding = 4) uniform subpassInput InKsPh;
layout(input_attachment_index = 5, set = 0, binding = 5) uniform subpassInput InColorTrans;

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

void main( void )
{
  ivec2 screen = ivec2(gl_FragCoord.xy);

  vec4 Pos = subpassLoad(InPos);
  vec4 NIsShade = subpassLoad(InNIsShade);
  vec4 Ka = subpassLoad(InKa);
  vec4 Kd = subpassLoad(InKd);
  vec4 KsPh = subpassLoad(InKsPh);

  vec3 Norm = NIsShade.xyz;
  bool IsShade = NIsShade.w > 0;

  vec3 Ks = KsPh.xyz;
  float Ph = KsPh.w;

  vec3 color = Shade(vec3(1, 1, 1), vec3(1, 1, 1), Pos.xyz, Norm, Kd.xyz, Ks, Ph);

  OutColor = vec4(color, 1.0);
}
