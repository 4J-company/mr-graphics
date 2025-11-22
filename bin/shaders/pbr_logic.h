struct PointData {
  vec3 LightDir;
  vec3 LightColor;

  vec3 Pos;
  vec3 Norm;
};

#define PI 3.14159265359

float DistributionGGX( vec3 N, vec3 H, float roughness )
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 ShadePBR( PointData point_data,
               vec3 color,
               vec3 emissive,
               float occlusion,
               float metallic,
               float roughness )
{
  vec3 outcolor = color.xyz;

  vec3 N = normalize(point_data.Norm.xyz);
  // TODO(dk6): don't use cam_uniform_buffer, add camera position to function arguments
  vec3 V = normalize(point_data.Pos - cam_uniform_buffer.pos.xyz);
  vec3 L = normalize(point_data.LightDir);
  vec3 H = normalize(V + L);
  float NdotL = max(dot(N, L), 0.0);
  float NdotV = max(dot(N, V), 0.0);
  float HdotV = max(dot(H, V), 0.0);

  vec3 F0 = vec3(0.04);
  F0 = mix(F0, color, metallic);

  vec3 radiance = point_data.LightColor;

  // Cook-Torrance BRDF
  float NDF = DistributionGGX(N, H, roughness);
  float G   = GeometrySmith(N, V, L, roughness);
  vec3 F    = fresnelSchlick(HdotV, F0);

  vec3 numerator    = NDF * G * F;
  float denominator = 4.0 * NdotV * NdotL + 0.0001;
  vec3 specular = numerator / denominator;

  vec3 kS = F;
  vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

  // final
  outcolor = (kD * color / PI + specular) * radiance * NdotL;

  outcolor = emissive + occlusion * outcolor;

  return outcolor;
}
