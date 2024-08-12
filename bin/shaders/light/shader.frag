/**/
#version 460

layout(location = 0) out vec4 OutColor;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput InPos;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput InNIsShade;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput InMR;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput InEmissive;
layout(input_attachment_index = 4, set = 0, binding = 4) uniform subpassInput InOcclusion;
layout(input_attachment_index = 5, set = 0, binding = 5) uniform subpassInput InColorTrans;

vec3 ShadePhong( vec3 LightDir, vec3 LightColor, vec3 P, vec3 N, vec3 Kd, vec3 Ks, float Ph )
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

struct PointData {
  vec3 LightDir;
  vec3 LightColor;

  vec3 Pos;
  vec3 Norm;
};

vec3 ShadePBR( PointData point_data, vec4 color, vec4 emissive, vec4 occlusion, float metallic, float roughness )
{
  roughness *= roughness;
  vec3 diffuse = color.xyz * (1.0 - metallic);
  vec3 specular = mix(vec3(0.0), color.xyz, metallic);
  float reflectance = max(max(specular.r, specular.g), specular.b);

  vec3 F = vec3(0.0);
  float G = 0.0;
  float D = 0.0;

  // diffuse = (1.0 - F);
  // specular = F * G * D / (4.0 * dot(n, v) * dot(n, l));

  return color.xyz;
}

void main( void )
{
  vec4 pos = subpassLoad(InPos);
  vec4 color = subpassLoad(InColorTrans);
  vec4 norm = subpassLoad(InNIsShade);

  vec4 roughness_metallic = subpassLoad(InMR);
  vec4 emissive = subpassLoad(InEmissive);
  vec4 occlusion = subpassLoad(InOcclusion);

  float metallic = roughness_metallic.b;
  float roughness = roughness_metallic.g;

  PointData pd = PointData(vec3(1.0), vec3(0.8, -0.47, 1.0), pos.xyz, norm.xyz);

  OutColor = vec4(ShadePBR(pd, color, emissive, occlusion, metallic, roughness), 1.0);
}
