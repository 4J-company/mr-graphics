struct PointData {
  vec3 LightPos;
  vec3 LightColor;

  vec3 Pos;
  vec3 Norm;
};

#define PI 3.14159265359

/**
 * @brief Computes the GGX normal distribution function.
 *
 * Evaluates the Trowbridge-Reitz microfacet distribution for a surface based on its normal (N),
 * half-vector (H), and roughness. The computation uses the square of the roughness to determine the
 * density of microfacets oriented in the direction of H, which is essential for physically-based
 * rendering calculations.
 *
 * @param N Surface normal vector.
 * @param H Half-vector between the view and light directions.
 * @param roughness Surface roughness factor; higher values yield a broader distribution.
 * @return float The computed microfacet distribution value.
 */
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

/**
 * @brief Computes the Schlick-GGX geometric attenuation factor.
 *
 * This function calculates the geometry term used in physically-based rendering
 * by applying the Schlick approximation. It adjusts the attenuation based on the
 * cosine of the angle between the surface normal and view direction (NdotV) and 
 * the surface roughness.
 *
 * @param NdotV The cosine of the angle between the surface normal and the view vector.
 * @param roughness The surface roughness; higher values lead to increased attenuation.
 * @return float The computed geometric attenuation factor.
 */
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

/**
 * @brief Computes the Fresnel reflectance using Schlick's approximation.
 *
 * Given the cosine of the incident angle (cosTheta) and the base reflectivity (F0) at normal incidence,
 * this function returns the Fresnel reflectance as a vec3.
 *
 * @param cosTheta Cosine of the angle between the view direction and the surface normal.
 * @param F0 Base reflectivity at normal incidence.
 * @return vec3 The computed Fresnel reflectance.
 */
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

/**
 * @brief Calculates the combined geometric attenuation factor for view and light directions.
 *
 * This function computes the attenuation factor using the Schlick GGX approximation for both 
 * the view and light vectors by first clamping the dot products of the surface normal with each 
 * direction, then multiplying the resulting factors.
 *
 * @param N Surface normal vector.
 * @param V View direction vector.
 * @param L Light direction vector.
 * @param roughness Surface roughness value that affects the attenuation.
 *
 * @return float The product of the geometric attenuation factors for the view and light directions.
 */
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

/**
 * @brief Shades a surface point using physically based rendering (PBR).
 *
 * Computes the final color of a surface point by applying the Cook-Torrance BRDF, which
 * combines diffuse and specular reflections. It calculates microfacet distribution, geometric
 * attenuation, and Fresnel reflectance, then applies tone mapping and gamma correction to produce
 * a physically plausible final color.
 *
 * @param point_data Contains surface and lighting information (surface position, normal, light position, and light color).
 * @param color      Base albedo of the surface.
 * @param emissive   Emissive color added to simulate self-illumination.
 * @param occlusion  Ambient occlusion factor that modulates the overall lighting.
 * @param metallic   Factor indicating the metallic property, influencing specular reflection.
 * @param roughness  Surface roughness that affects microfacet distribution and geometric calculations.
 *
 * @return vec3      Final shaded color.
 */
vec3 ShadePBR( PointData point_data,
               vec3 color,
               vec3 emissive,
               float occlusion,
               float metallic,
               float roughness )
{
  vec3 outcolor = color.xyz;

  vec3 N = normalize(point_data.Norm.xyz);
  vec3 V = normalize(point_data.Pos - cam_ubo.pos.xyz);
  vec3 L = normalize(point_data.LightPos - point_data.Pos);
  vec3 H = normalize(V + L);
  float NdotL = max(dot(N, L), 0.0);
  float NdotV = max(dot(N, V), 0.0);
  float HdotV = max(dot(H, V), 0.0);

  vec3 F0 = vec3(0.04);
  F0 = mix(F0, color, metallic);

  float distance = length(point_data.LightPos - point_data.Pos);
  float attenuation = 1.0 / (distance * distance);
  vec3 radiance = point_data.LightColor * attenuation;

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

  // tone mapping
  outcolor = outcolor / (outcolor + vec3(1.0));

  // gamma correction
  outcolor = pow(outcolor, vec3(1.0/2.2));

  return outcolor;
}
