/**
 * @brief Computes the Phong shading color for a 3D point.
 *
 * This function calculates the color at a given point by combining diffuse and specular components
 * using the Phong lighting model. It adjusts the light direction by inverting its y-component,
 * computes the view vector from the point to the fixed position (1, 1, 1), and uses the faceforward
 * function to ensure the normal vector faces the view direction.
 *
 * The diffuse component is determined by clamping the dot product between the normal and light
 * direction to a minimum of 0.1, then scaling by the diffuse coefficient (Kd) and the light color.
 * The specular component is calculated by reflecting the view vector about the normal, clamping the
 * resulting dot product with the light direction to a minimum of 0.1, raising it to the power of the
 * shininess exponent (Ph), and scaling by the specular coefficient (Ks) and the light color.
 *
 * @param LightDir Direction of the incoming light.
 * @param LightColor Color and intensity of the light.
 * @param P Position of the point being shaded.
 * @param N Surface normal at the point.
 * @param Kd Diffuse reflection coefficient.
 * @param Ks Specular reflection coefficient.
 * @param Ph Shininess exponent controlling specular highlight sharpness.
 *
 * @return vec3 Computed color after combining diffuse and specular lighting contributions.
 */
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
