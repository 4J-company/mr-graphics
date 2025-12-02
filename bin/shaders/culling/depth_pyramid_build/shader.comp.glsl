#version 460

layout(local_size_x = THREADS_NUM, local_size_y = THREADS_NUM, local_size_z = 1) in;

// TODO: correct layouts
layout(set = 2, binding = 8, r32f) uniform writeonly image2D InImage;
layout(set = 2, binding = 8, r32f) uniform readonly image2D OutImage;

layout(push_constant) uniform PushContants {
  uint out_level;
  uint in_width;
  uint in_heigth;
} data;

void main()
{
  uvec2 coord = gl_GlobalInvocationID.xy;
}