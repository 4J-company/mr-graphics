vec3 tm_aces(vec3 x) {
  float a = 2.51f;
  float b = 0.03f;
  float c = 2.43f;
  float d = 0.59f;
  float e = 0.14f;
  return clamp(
    (x * (a * x + b)) /
      (x * (c * x + d) + e),
    0.f, 1.f);
}

vec3 tm_linear(vec3 x) {
  return x / (x + vec3(1.0));
}
