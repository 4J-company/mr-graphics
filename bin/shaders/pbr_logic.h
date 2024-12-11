vec4 calcDirLight() {
  return vec4(0);
}

vec4 calcPointLight() {
  return vec4(0);
}

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
