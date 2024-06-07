#if !defined(__descriptor_hpp_)
#define __descriptor_hpp_

#include "pch.hpp"
#include "resources/attachment/attachment.hpp"
#include "resources/images/image.hpp"
#include "resources/pipelines/pipeline.hpp"
#include "resources/texture/sampler/sampler.hpp"

namespace mr {
  class Descriptor {
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
                 const std::vector<DescriptorAttachment> &attachments,
                 uint set_number = 0);

      Descriptor(Descriptor &&other) noexcept = default;
      Descriptor &operator=(Descriptor &&other) noexcept = default;

    private:
      void create_descriptor_pool(
        const VulkanState &state,
        const std::vector<DescriptorAttachment> &attachments);

    public:
      void update_all_attachments(
        const VulkanState &state,
        const std::vector<DescriptorAttachment> &attachments);

      void apply();

      const vk::DescriptorSet set() { return _set; }
  };
} // namespace mr

#endif // __descriptor_hpp_
