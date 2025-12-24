#version 460

layout(location = 0) out flat uint instance_index;

void main()
{
    gl_Position = vec4(0, 0, 0, 1.0);
    instance_index = gl_InstanceIndex;
}
