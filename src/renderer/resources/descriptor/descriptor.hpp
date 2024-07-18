#if !defined(__descriptor_hpp_)
#define __descriptor_hpp_

#include "pch.hpp"
#include "resources/attachment/attachment.hpp"
#include "resources/images/image.hpp"
#include "resources/pipelines/pipeline.hpp"
#include "resources/texture/sampler/sampler.hpp"

namespace mr {
  class UniformBuffer;
  class StorageBuffer;
  class Texture;
  class Image;

  class Descriptor {
  public:

    struct Attachment {
      using Data = std::variant<UniformBuffer *, StorageBuffer *, Texture *, Image *>;
      Data data;
      uint32_t binding;
      uint32_t set;
      vk::ShaderStageFlags shader_stages;
      vk::DescriptorType descriptor_type;
    };

    struct Constant {
      uint32_t size;
      uint32_t offset;
      vk::ShaderStageFlags shader_stages;
    };

  private:
    vk::UniqueDescriptorPool _pool;

    /// TODO: static allocation
    /// inline static const int _max_sets = 4;
    /// std::array<vk::DescriptorSet, _max_sets> _sets;
    /// std::array<vk::DescriptorSetLayout, _max_sets> _set_layouts;

    vk::DescriptorSet _set;
    vk::DescriptorSetLayout _set_layout;
    uint _set_number;

  public:
    Descriptor() = default;

    Descriptor(const VulkanState &state, Pipeline *pipeline,
               const std::vector<Attachment::Data> &attachments,
               uint set_number = 0);

    void update_all_attachments(
      const VulkanState &state,
      const std::vector<Attachment::Data> &attachments);

    void apply();

    vk::DescriptorSet set() const noexcept { return _set; }

    private:
      void
      create_descriptor_pool(const VulkanState &state,
                             const std::vector<Attachment::Data> &attachments);
  };

  vk::DescriptorType
  get_descriptor_type(const Descriptor::Attachment::Data &attachment) noexcept;
} // namespace mr

#endif // __descriptor_hpp_
