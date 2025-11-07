#ifndef __MR_VULKAN_STATE_HPP_
#define __MR_VULKAN_STATE_HPP_

#include "pch.hpp"
#include <VkBootstrap.h>

namespace mr {
inline namespace graphics {
  class VulkanGlobalState {
    private:
      // these resources are shared between all VulkanStates
      friend class VulkanState;
      vkb::Instance _instance;
      vkb::PhysicalDevice _phys_device;

      CacheFile _pipeline_cache_file;

      // VulkanGlobalState is managed by Application
      friend class Application;
      VulkanGlobalState(bool init_vkfw = true);
      ~VulkanGlobalState();

      void _create_instance();
      void _create_phys_device();
  };

  class VulkanState {
    private:
      VulkanGlobalState *_global;
      vkb::Device _device;
      vkb::DispatchTable _dispatch_table;
      vk::Queue _queue;
      vk::UniquePipelineCache _pipeline_cache;
      VmaAllocator _allocator;

    public:
      VulkanState() = default;
      VulkanState(VulkanState &&) = default;
      VulkanState &operator=(VulkanState &&) = default;

      explicit VulkanState(VulkanGlobalState *state);
      ~VulkanState();

      vk::Instance instance() const noexcept { return _global->_instance.instance; }
      vk::PhysicalDevice phys_device() const noexcept { return _global->_phys_device.physical_device; }
      vk::Device device() const noexcept { return _device.device; }
      vkb::DispatchTable dispatch_table() const noexcept { return _dispatch_table; }
      vk::Queue queue() const noexcept { return _queue; }
      vk::PipelineCache pipeline_cache() const noexcept { return *_pipeline_cache; }
      VmaAllocator allocator() const noexcept { return _allocator; }
#ifndef NDEBUG
      const VmaBudget * memory_budgets() const noexcept;
#endif

    private:
      void _create_device();
      void _create_allocator();
      void _create_pipeline_cache();
      void _destroy_pipeline_cache();
  };
}
} // namespace mr

#endif // __MR_VULKAN_STATE_HPP_
