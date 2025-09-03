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
