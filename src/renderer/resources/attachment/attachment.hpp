#if !defined(__attachment_hpp_)
#define __attachment_hpp_

namespace mr {
  class Buffer;
  class Texture;
  class Image;

  // TODO: trying convert it into union
  struct DescriptorAttachment {
      Buffer *uniform_buffer {};
      Buffer *storage_buffer {};
      Texture *texture {};
      Image *gbuffer {};
      // const ShadowMap *shadow_map = nullptr;
  };

  // TODO: maybe delete
  struct Attachment {
      DescriptorAttachment data;

      uint32_t binding;
      uint32_t set;

      vk::ShaderStageFlags usage_flags;
      vk::DescriptorType descriptor_type;
  };

  struct Constant {
      uint32_t size;
      uint32_t offset;
      vk::ShaderStageFlags usage_flags;
  };
} // namespace mr

#endif // __attachment_hpp_
