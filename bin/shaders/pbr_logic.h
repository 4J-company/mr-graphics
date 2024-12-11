struct PointData {
  vec3 LightDir;
  vec3 LightColor;

  vec3 Pos;
  vec3 Norm;
};

vec3 ShadePBR( PointData point_data,
               vec4 color,
               vec4 emissive,
               vec4 occlusion,
               float metallic,
               float roughness )
{
  vec3 outcolor = color.xyz;

  roughness *= roughness;
  vec3 diffuse = color.xyz * (1.0 - metallic);
  vec3 specular = mix(vec3(0.0), color.xyz, metallic);
  float reflectance = max(max(specular.r, specular.g), specular.b);

  vec3 F = vec3(0.0);
  float G = 0.0;
  float D = 0.0;

  // diffuse = (1.0 - F);
  // specular = F * G * D / (4.0 * dot(n, v) * dot(n, l));

  // tone mapping
  outcolor = outcolor / (outcolor + vec3(1.0));

  // gamma correction
  outcolor = pow(outcolor, vec3(1.0/2.2));

  return outcolor;
}
