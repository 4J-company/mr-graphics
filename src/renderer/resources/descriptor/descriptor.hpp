#ifndef __MR_DESCRIPTOR_HPP_
#define __MR_DESCRIPTOR_HPP_

#include "pch.hpp"
#include "resources/images/image.hpp"
#include "resources/pipelines/pipeline.hpp"
#include "resources/shaders/shader.hpp"
#include "resources/texture/sampler/sampler.hpp"

namespace mr {
  class DescriptorAllocator;

  class DescriptorSet {
      friend class DescriptorAllocator;

    public:
      DescriptorSet() noexcept = default;
      DescriptorSet(DescriptorSet &&) noexcept = default;
      DescriptorSet &operator=(DescriptorSet &&) noexcept = default;

      DescriptorSet(vk::DescriptorSet set, vk::UniqueDescriptorSetLayout layout,
                    uint number) noexcept
          : _set(set)
          , _set_layout(std::move(layout))
          , _set_number(number)
      {
      }

      void update(const VulkanState &state,
                  std::span<const Shader::ResourceView> attachments) noexcept;

      vk::DescriptorSet set() const noexcept { return _set; }

      operator vk::DescriptorSet() const noexcept { return _set; }

      vk::DescriptorSetLayout layout() const noexcept
      {
        return _set_layout.get();
      }

    private:
      vk::DescriptorSet _set;
      vk::UniqueDescriptorSetLayout _set_layout;
      uint _set_number;
  };

  class DescriptorAllocator {
    private:
      std::vector<vk::UniqueDescriptorPool> _pools;
      const VulkanState &_state;

      std::optional<vk::UniqueDescriptorPool>
      allocate_pool(std::span<vk::DescriptorPoolSize> sizes) noexcept;

    public:
      DescriptorAllocator(const VulkanState &state);

      std::optional<mr::DescriptorSet>
      allocate_set(Shader::Stage stage,
                   std::span<const Shader::ResourceView> attachments) noexcept;

      std::optional<std::vector<mr::DescriptorSet>> allocate_sets(
        std::span<
          const std::pair<Shader::Stage, std::span<const Shader::ResourceView>>>
          attachment_sets) noexcept;

      void reset() noexcept;
  };
} // namespace mr

#endif // __MR_DESCRIPTOR_HPP_
