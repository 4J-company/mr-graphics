#version 460

layout(location = 0) out vec4 OutPos;
layout(location = 1) out vec4 OutNIsShade;
layout(location = 2) out vec4 OutMR;
layout(location = 3) out vec4 OutEmissive;
layout(location = 4) out vec4 OutOcclusion;
layout(location = 5) out vec4 OutColorTrans;

#ifdef ENABLE_BOUNDS_FRAG_COLOR
layout(location = 1) in flat uint vertex_color;
#endif // ENABLE_BOUNDS_FRAG_COLOR

void main() {
  OutNIsShade = vec4(vec3(0), 0);
#ifdef ENABLE_BOUNDS_FRAG_COLOR
  OutColorTrans = vec4(
    ((vertex_color & 0xFF000000) >> 24) / 255.0,
    ((vertex_color & 0x00FF0000) >> 16) / 255.0,
    ((vertex_color & 0x0000FF00) >>  8) / 255.0,
    ((vertex_color & 0x000000FF) >>  0) / 255.0
  );
#else // ENABLE_BOUNDS_FRAG_COLOR
  OutColorTrans = vec4(1, 0, 0, 1);
#endif // ENABLE_BOUNDS_FRAG_COLOR
}
