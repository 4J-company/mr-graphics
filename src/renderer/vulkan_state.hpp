#ifndef __MR_VULKAN_STATE_HPP_
#define __MR_VULKAN_STATE_HPP_

#include "pch.hpp"
#include "utils/misc.hpp"
#include <VkBootstrap.h>

namespace mr {
  class VulkanGlobalState {
    private:
      // VulkanGlobalState is managed by Application
      friend class Application;
      VulkanGlobalState();
      ~VulkanGlobalState();

      void _create_instance();
      void _create_phys_device();

      // these resources are shared between all VulkanStates
      friend class VulkanState;
      vkb::Instance _instance;
      vkb::PhysicalDevice _phys_device;

      CacheFile _pipeline_cache;
  };

  class VulkanState {
    public:
      VulkanState() = default;
      VulkanState(VulkanState &&) = default;
      VulkanState &operator=(VulkanState &&) = default;

      explicit VulkanState(VulkanGlobalState *state);
      ~VulkanState();

      vk::Instance instance() const noexcept { return _global->_instance.instance; }
      vk::PhysicalDevice phys_device() const noexcept { return _global->_phys_device.physical_device; }
      vk::Device device() const noexcept { return *_device; }
      /**
 * @brief Retrieves the Vulkan queue used for submitting commands.
 *
 * This method returns the Vulkan queue handle maintained within the state, which is used for command submissions.
 *
 * @return vk::Queue The Vulkan queue.
 */
vk::Queue queue() const noexcept { return _queue; }
      /**
 * @brief Retrieves the Vulkan pipeline cache.
 *
 * This function returns the pipeline cache associated with the current Vulkan state by 
 * dereferencing its internal unique pointer.
 *
 * @return vk::PipelineCache The pipeline cache.
 */
vk::PipelineCache pipeline_cache() const noexcept { return *_pipeline_cache; }

    private:
      void _create_device();
      void _create_pipeline_cache();
      void _destroy_pipeline_cache();

      VulkanGlobalState *_global;
      vk::UniqueDevice _device;
      vk::Queue _queue;
      vk::UniquePipelineCache _pipeline_cache;
  };
} // namespace mr

#endif // __MR_VULKAN_STATE_HPP_
