#ifndef __MR_DESCRIPTOR_HPP_
#define __MR_DESCRIPTOR_HPP_

#include "pch.hpp"
#include "resources/shaders/shader.hpp"

namespace mr {
inline namespace graphics {
  class DescriptorAllocator;

  // TODO(dk6): It will be very cool to make BindlessDescriptorSet size dynamicly increased, but:
  //  For this:
  //  1) We must store registered resources (now we store only uintptr_t like hash).
  //     Maybe we can store write infos instead.
  //  2) We must recreate descriptor allocator, but we have no link on this in descriptor set.
  //     Maybe we can move vulkan descriptor allocator in set class instead creating set from
  //     descriptor allocator class.
  //  3) We must recreate descriptor set layout - and this is the biggest trouble because:
  //     1. We store set layout as readonly reference, because a lot of sets can use one layout
  //     2. If each descriptor set will have own unique set layout it follows big increasment of
  //        number of pipelines, but it is bad for indirect rendering
  //     3. After recreate descriptor set layout we must recreate all pipelines - it is very
  //        slow operation and very uncomfortable for supporting in code
  // So, decisions of 1) and 2) aren't very difficult, but 3) is an issue
  constexpr static uint32_t resource_max_number_per_binding = 4730;

  constexpr static uint32_t desciptor_set_max_bindings = 30;

  class DescriptorSetLayout : public ResourceBase<DescriptorSetLayout> {
  public:
    struct BindingDescription {
      uint32_t binding;
      vk::DescriptorType type;
    };

  protected:
    vk::UniqueDescriptorSetLayout _set_layout {};
    // Its 1200 bytes, it is too much for stack allocation
    std::vector<std::optional<vk::DescriptorType>> _bindings;

  public:
    DescriptorSetLayout() noexcept = default;

    DescriptorSetLayout(DescriptorSetLayout &&) noexcept = default;
    DescriptorSetLayout & operator=(DescriptorSetLayout &&) noexcept = default;

    DescriptorSetLayout(const VulkanState &state,
                        const vk::ShaderStageFlags stage,
                        std::span<const BindingDescription> bindings) noexcept;

    vk::DescriptorSetLayout layout() const noexcept { return _set_layout.get(); }
    std::span<const std::optional<vk::DescriptorType>> bindings() const noexcept { return _bindings; }

  protected:
    using CreateBindingsList = InplaceVector<vk::DescriptorSetLayoutBinding, desciptor_set_max_bindings>;
    void fill_binding(CreateBindingsList &set_bindings,
                      const vk::ShaderStageFlags stage,
                      std::span<const BindingDescription> bindings,
                      uint32_t descriptor_count = 1) noexcept;
  };
  MR_DECLARE_HANDLE(DescriptorSetLayout);

  class BindlessDescriptorSetLayout : public DescriptorSetLayout, public ResourceBase<BindlessDescriptorSetLayout> {
  public:
    BindlessDescriptorSetLayout() noexcept = default;

    BindlessDescriptorSetLayout(BindlessDescriptorSetLayout &&) noexcept = default;
    BindlessDescriptorSetLayout & operator=(BindlessDescriptorSetLayout &&) noexcept = default;

    BindlessDescriptorSetLayout(const VulkanState &state,
                                const vk::ShaderStageFlags stage,
                                std::span<const BindingDescription> bindings) noexcept;
  };
  MR_DECLARE_HANDLE(BindlessDescriptorSetLayout);

  class DescriptorSet {
    friend class DescriptorAllocator;

  private:
    vk::DescriptorSet _set;
    DescriptorSetLayoutHandle _set_layout;

  public:
    DescriptorSet() = default;

    DescriptorSet(DescriptorSet &&) noexcept = default;
    DescriptorSet & operator=(DescriptorSet &&) noexcept = default;

    DescriptorSet(vk::DescriptorSet set, DescriptorSetLayoutHandle layout) noexcept
      : _set(set), _set_layout(std::move(layout)) {}

    void update(const VulkanState &state,
                std::span<const Shader::ResourceView> attachments) noexcept;

    vk::DescriptorSet set() const noexcept { return _set; }
    const DescriptorSetLayoutHandle& layout_handle() const noexcept { return _set_layout; }

    operator vk::DescriptorSet() const noexcept { return _set; }
  };

  class BindlessDescriptorSet {
    friend class DescriptorAllocator;

  private:
    class ResourcePoolData {
      struct ResourceStat {
        // if mutex will be removed this variable must be atomic
        uint32_t id = -1;
        uint32_t usage_count = 0;
      };

    private:
      std::atomic_uint32_t current_id = 0;
      InplaceVector<uint32_t, resource_max_number_per_binding> free_ids;
      boost::unordered_map<std::uintptr_t, ResourceStat> usage;
      // Mutex may be bottle neck, it require profiling. Now it used for simple hanlde consistency
      // of two data sctuctures (vector and map)
      std::mutex mutex;
      uint32_t max_number;

    public:
      ResourcePoolData(uint32_t max_resource_number = resource_max_number_per_binding) noexcept;

      ResourcePoolData(ResourcePoolData &&other) noexcept;
      ResourcePoolData & operator=(ResourcePoolData &&other) noexcept;

      uint32_t get_id(std::uintptr_t resource) noexcept;
      void unregister(std::uintptr_t resource) noexcept;
    };

    using ResourceInfo = std::variant<vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;

    using TypeBindingPair = std::pair<vk::DescriptorType, uint32_t>;
    // we will use not more than 15 descriptor types
    static constexpr uint32_t unique_types_array_size = 15;
    using UniqueDesciptorTypes = InplaceVector<TypeBindingPair, unique_types_array_size>;

  private:
    vk::UniqueDescriptorSet _set;
    BindlessDescriptorSetLayoutHandle _set_layout;
    const VulkanState *_state = nullptr;

    UniqueDesciptorTypes _unique_resources_types;
    // We know it max size - desciptor_set_max_bindings. But size of this array is 100kb,
    // this is too much for stack allocation, so we use heap memory with one time allocation
    // in consturctor
    std::vector<ResourcePoolData> _resource_pools;
    // It is necessary for deletion. We can delete this if change interface of 'unregister_resource`: change parameter
    // from Resource to ResourceView, with data of binding. But it loops like dirty interface, I think deletion of
    // resource must be easy
    boost::unordered_map<std::uintptr_t, uint32_t> _bindings_of_resources;

  public:
    BindlessDescriptorSet() = default;

    BindlessDescriptorSet(BindlessDescriptorSet &&) noexcept = default;
    BindlessDescriptorSet & operator=(BindlessDescriptorSet &&) noexcept = default;

    BindlessDescriptorSet(const VulkanState &state,
                          vk::UniqueDescriptorSet set,
                          BindlessDescriptorSetLayoutHandle layout) noexcept;

    uint32_t register_resource(const Shader::Resource &resource) noexcept;
    uint32_t register_resource(const Shader::ResourceView &resource_view) noexcept;
    // If RVO here doesn't work it will be coping 120 bytes.
    // It is not a lot, but maybe this interface can be refactored
    InplaceVector<uint32_t, desciptor_set_max_bindings> register_resources(
      std::span<const Shader::Resource> resources) noexcept;

    void unregister_resource(const Shader::Resource &resource) noexcept;

    vk::DescriptorSet set() const noexcept { return _set.get(); }
    const BindlessDescriptorSetLayoutHandle & layout_handle() const noexcept { return _set_layout; }

    operator vk::DescriptorSet() const noexcept { return _set.get(); }

  private:
    void fill_texture(const Texture *texture,
                      vk::DescriptorImageInfo &image_info) const noexcept;
    void fill_uniform_buffer(const UniformBuffer *buffer,
                             vk::DescriptorBufferInfo &buffer_info) const noexcept;
    void fill_storage_buffer(const StorageBuffer *buffer,
                             vk::DescriptorBufferInfo &buffer_info) const noexcept;
    uint32_t fill_resource(const Shader::ResourceView &resource,
                           ResourceInfo &resource_info,
                           vk::WriteDescriptorSet &write_info) noexcept;

    Shader::ResourceView try_convert_view_to_resource(const Shader::Resource &resource) const noexcept;
  };

  class DescriptorAllocator {
  private:
    std::vector<vk::UniqueDescriptorPool> _pools;
    vk::UniqueDescriptorPool _bindless_pool;
    const VulkanState *_state {};

  public:
    DescriptorAllocator(const VulkanState &state);

    std::optional<mr::DescriptorSet> allocate_set(DescriptorSetLayoutHandle set_layout) const noexcept;

    std::optional<std::vector<mr::DescriptorSet>> allocate_sets(
      std::span<const DescriptorSetLayoutHandle> set_layouts) const noexcept;

    std::optional<BindlessDescriptorSet> allocate_bindless_set(
      BindlessDescriptorSetLayoutHandle set_layout) const noexcept;

    void reset() noexcept;

    DescriptorAllocator & operator=(DescriptorAllocator &&) noexcept = default;
    DescriptorAllocator(DescriptorAllocator &&) noexcept = default;

  private:
    std::optional<vk::UniqueDescriptorPool> allocate_pool(
        std::span<const vk::DescriptorPoolSize> sizes,
        bool bindless = false) noexcept;
  };
}
} // namespace mr

#endif // __MR_DESCRIPTOR_HPP_
