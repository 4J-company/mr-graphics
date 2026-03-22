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

  bool last_level = data.dst_size.x == 1 && data.dst_size.y == 1;

  if (!last_level) {
    // TODO(dk6): Maybe size of depth pyramid must me pow of 2 closest to screen size
    vec2 tex_coord = (vec2(coord) + vec2(0.5)) / data.dst_size;

    // Real size of texture can be bigger - for simple resize we have a size of the biggest monitor
    tex_coord /= data.real_src_size;
    tex_coord *= data.src_size;

    // Using max sampler for texture
    float depth = texture(SrcImage, tex_coord).r;

    imageStore(DstImage, ivec2(coord), vec4(depth));
  } else {
    // Workaround for situation when prelast level is 3x2 and last is 1x1 and max sampler doesn't catch 1.0 value
    // TODO(dk6): this error also actual for 5x2 predlast level in 2K resolution
    vec2 offsets[] = {vec2(0), vec2(1)};
    float depth = 1;
    for (int i = 0; i < 2; i++) {
      vec2 tex_coord = (vec2(coord) + offsets[i]) / data.dst_size;

      // Real size of texture can be bigger - for simple resize we have a size of the biggest monitor
      tex_coord /= data.real_src_size;
      tex_coord *= data.src_size;

      depth = max(depth, texture(SrcImage, tex_coord).r);
    }
    imageStore(DstImage, ivec2(coord), vec4(depth));
  }
}
