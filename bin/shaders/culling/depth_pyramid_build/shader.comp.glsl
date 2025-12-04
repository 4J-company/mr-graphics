#version 460

layout(local_size_x = THREADS_NUM, local_size_y = THREADS_NUM, local_size_z = 1) in;

// TODO: correct layouts
layout(set = 2, binding = 8, r32f) uniform writeonly image2D InImage;
layout(set = 2, binding = 8, r32f) uniform readonly image2D OutImage;

layout(push_constant) uniform PushContants {
  uint out_level;
  uvec2 in_size;
} data;

void main()
{
  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
  vec2 avg_coord = (vec2(coord) + vec2(0.5)) / vec2(data.in_size);
  vec4 depth_avg = textureGather(ImImage, avg_coor);
  float depth = min(min(depth_avg.x, depth_avg.y), min(depth_avg.z, depth_avg.w));
  imageStore(OutImage, coord / 2, vec4(depth));
}