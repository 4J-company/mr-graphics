/**/
#version 460

layout(location = 0) out vec4 OutColor;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput InPos;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput InNIsShade;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput InKa;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput InKd;
layout(input_attachment_index = 4, set = 0, binding = 4) uniform subpassInput InKsPh;
layout(input_attachment_index = 5, set = 0, binding = 5) uniform subpassInput InColorTrans;

void main( void )
{
  ivec2 screen = ivec2(gl_FragCoord.xy);
 
  vec4 Kd = subpassLoad(InKd);
  OutColor = Kd;
}
