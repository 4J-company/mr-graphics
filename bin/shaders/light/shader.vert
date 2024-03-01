/**/
#version 460
layout(location = 0) in vec2 InPosition;

void main( void )
{
  gl_Position = vec4(InPosition, 0, 1);
}
