#if !defined(__vulkan_application_hpp_)
  #define __vulkan_application_hpp_

  #include "pch.hpp"

namespace mr
{
  class VulkanState
  {
    friend class Application;
  private:
    vk::Instance _instance;
    vk::Device _device;
    vk::PhysicalDevice _phys_device;
    vk::Queue _queue;

  public:
    VulkanState() = default;

    vk::Instance instance() const { return _instance; }
    vk::PhysicalDevice phys_device() const { return _phys_device; }
    vk::Device device() const { return _device; }
    vk::Queue queue() const { return _queue; }
  };
} // namespace mr

#endif // __vulkan_application_hpp_
