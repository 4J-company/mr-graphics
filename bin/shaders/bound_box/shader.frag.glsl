#version 460

layout(location = 0) out vec4 OutPos;
layout(location = 1) out vec4 OutNIsShade;
layout(location = 2) out vec4 OutMR;
layout(location = 3) out vec4 OutEmissive;
layout(location = 4) out vec4 OutOcclusion;
layout(location = 5) out vec4 OutColorTrans;

void main() {
  OutNIsShade = vec4(0, 0, 0, 1);
  OutColorTrans = vec4(1, 0, 0, 1);
}

