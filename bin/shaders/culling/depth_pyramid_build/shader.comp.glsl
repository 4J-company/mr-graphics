#version 460

#extension GL_EXT_nonuniform_qualifier : enable
// #extension GL_ARB_texture_gather : enable

layout(local_size_x = THREADS_NUM, local_size_y = THREADS_NUM, local_size_z = 1) in;
layout(push_constant) uniform PushContants {
  uint src_image;
  uint dst_image;
  uvec2 dst_size;
} data;

// layout(set = BINDLESS_SET, binding = INPUT_ATTACHMENTS) uniform readonly InputAttachments[];
layout(set = BINDLESS_SET, binding = STORAGE_IMAGES_BINDING, r32f) uniform image2D StorageImages[];

// #define SrcImage (data.dst_image == 0 ? InputAttachments[data.src_image] ? StorageImages[data.src_image])
#define SrcImage StorageImages[data.src_image]
#define DstImage StorageImages[data.dst_image]

void main()
{
  uvec2 coord = gl_GlobalInvocationID.xy;
  if (coord.x >= data.dst_size.x || coord.y >= data.dst_size.y) {
    return;
  }

  // TODO(dk6): maybe use sampler for all mips
  // vec2 avg_coord = (vec2(coord) + vec2(0.5)) / vec2(data.in_size);
  // vec4 depth_avg = textureGather(SrcImage, avg_coord);

  ivec2 read_coord = ivec2(coord) * 2;
  vec4 depth_avg = vec4(
    imageLoad(SrcImage, read_coord + ivec2(0, 0)).r,
    imageLoad(SrcImage, read_coord + ivec2(0, 1)).r,
    imageLoad(SrcImage, read_coord + ivec2(1, 0)).r,
    imageLoad(SrcImage, read_coord + ivec2(1, 1)).r
  );
  float depth = min(min(depth_avg.x, depth_avg.y), min(depth_avg.z, depth_avg.w));
  imageStore(DstImage, ivec2(coord), vec4(depth));
}
