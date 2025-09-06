#ifndef __MR_DESCRIPTOR_HPP_
#define __MR_DESCRIPTOR_HPP_

#include "pch.hpp"
#include "resources/images/image.hpp"
#include "resources/shaders/shader.hpp"
#include "resources/texture/sampler/sampler.hpp"

namespace mr {
inline namespace graphics {
  class DescriptorAllocator;

  class DescriptorSetLayout : public ResourceBase<DescriptorSetLayout> {
  private:
    vk::UniqueDescriptorSetLayout _set_layout {};

  public:
    DescriptorSetLayout() noexcept = default;

    DescriptorSetLayout(DescriptorSetLayout &&) noexcept = default;
    DescriptorSetLayout & operator=(DescriptorSetLayout &&) noexcept = default;

    // Theoreticaly, there is opportunity to create set layout without valid resourceses on hands.
    // For example to create set layout for one UBO you can pass to attachments
    //   {Shader::ResourceView(0, 0, static_cast<UniformBuffer *>(nullptr))}
    // But now this wasn't tested
    // Also shader reflexy can delete this?
    DescriptorSetLayout(const VulkanState &state,
                        const vk::ShaderStageFlags stage,
                        std::span<const Shader::ResourceView> attachments) noexcept;

    vk::DescriptorSetLayout layout() const noexcept { return _set_layout.get(); }
  };

  MR_DECLARE_HANDLE(DescriptorSetLayout)

  class DescriptorSet {
      friend class DescriptorAllocator;

    private:
      vk::DescriptorSet _set;
      DescriptorSetLayoutHandle _set_layout;
      uint32_t _set_number;

    public:
      DescriptorSet() noexcept = default;

      DescriptorSet(DescriptorSet &&) noexcept = default;
      DescriptorSet & operator=(DescriptorSet &&) noexcept = default;

      DescriptorSet(vk::DescriptorSet set, DescriptorSetLayoutHandle layout, uint number) noexcept
        : _set(set), _set_layout(layout), _set_number(number) {}

      void update(const VulkanState &state,
                  std::span<const Shader::ResourceView> attachments) noexcept;

      vk::DescriptorSet set() const noexcept { return _set; }
      DescriptorSetLayoutHandle layout_handle() const noexcept { return _set_layout; }
      uint32_t set_number() const noexcept { return _set_number; }

      operator vk::DescriptorSet() const noexcept { return _set; }
  };

  class DescriptorAllocator {
    private:
      std::vector<vk::UniqueDescriptorPool> _pools;
      const VulkanState *_state {};

    public:
      DescriptorAllocator(const VulkanState &state);

      std::optional<mr::DescriptorSet> allocate_set(DescriptorSetLayoutHandle set_layout) const noexcept;

      std::optional<std::vector<mr::DescriptorSet>> allocate_sets(
        std::span<const DescriptorSetLayoutHandle> set_layouts) const noexcept;

      void reset() noexcept;

      DescriptorAllocator & operator=(DescriptorAllocator &&) noexcept = default;
      DescriptorAllocator(DescriptorAllocator &&) noexcept = default;

    private:
      std::optional<vk::UniqueDescriptorPool> allocate_pool(std::span<const vk::DescriptorPoolSize> sizes) noexcept;
  };
}
} // namespace mr

#endif // __MR_DESCRIPTOR_HPP_
