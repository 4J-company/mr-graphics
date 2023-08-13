#if !defined(__attachment_hpp_)
  #define __attachment_hpp_

namespace ter
{
  class Buffer;
  class Image;
  class Sampler;

  union AttachmentData {
    const Buffer *buffer;
    const Image *image;
    const Sampler *sampler;
    // const ShadowMap *shadow_map = nullptr;
  };

  struct Attachment
  {
    AttachmentData data;

    uint32_t binding;
    uint32_t set;

    vk::ShaderStageFlags usage_flags;
    vk::DescriptorType descriptor_type;
  };

  struct Constant
  {
    uint32_t size;
    uint32_t offset;
    vk::ShaderStageFlags usage_flags;
  };
} // namespace ter

#endif // __attachment_hpp_
