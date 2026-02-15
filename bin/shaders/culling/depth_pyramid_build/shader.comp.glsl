#version 460

#extension GL_EXT_nonuniform_qualifier : enable
// #extension GL_ARB_texture_gather : enable

layout(local_size_x = THREADS_NUM, local_size_y = THREADS_NUM, local_size_z = 1) in;
layout(push_constant) uniform PushContants {
  uint src_image;
  uint dst_image;

  uvec2 dst_size;

  uvec2 src_size;
  uvec2 real_src_size;
} data;


layout(set = BINDLESS_SET, binding = TEXTURES_BINDING) uniform sampler2D SampledImages[];
layout(set = BINDLESS_SET, binding = STORAGE_IMAGES_BINDING, r32f) uniform image2D StorageImages[];

#define SrcImage SampledImages[data.src_image]
#define DstImage StorageImages[data.dst_image]

void main()
{
  uvec2 coord = gl_GlobalInvocationID.xy;
  if (coord.x >= data.dst_size.x || coord.y >= data.dst_size.y) {
    return;
  }

  // TODO(dk6): Maybe size of depth pyramid must me pow of 2 closest to screen size
  vec2 tex_coord_left_angle = vec2(coord) / data.dst_size;
  vec2 tex_coord_rigth_angle = (vec2(coord) + vec2(1)) / data.dst_size;

  // Real size of texture can be bigger - for simple resize we have a size of the biggest monitor
  tex_coord_left_angle /= data.real_src_size;
  tex_coord_left_angle *= data.src_size;
  tex_coord_rigth_angle /= data.real_src_size;
  tex_coord_rigth_angle *= data.src_size;

  // Using max sampler for texture
  float depth_left_angle = texture(SrcImage, tex_coord_left_angle).r;
  float depth_rigth_angle = texture(SrcImage, tex_coord_rigth_angle).r;
  float depth = max(depth_left_angle, depth_rigth_angle);

  imageStore(DstImage, ivec2(coord), vec4(depth));
}
