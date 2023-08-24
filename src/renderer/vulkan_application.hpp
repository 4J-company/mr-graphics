#include <vulkan/vulkan_handles.hpp>
#if !defined(__vulkan_application_hpp_)
  #define __vulkan_application_hpp_

  #include "pch.hpp"

namespace ter
{
  class VulkanState
  {
    friend class Application;
  private:
    vk::Instance _instance;
    vk::Device _device;
    vk::PhysicalDevice _phys_device;
    vk::RenderPass _render_pass;
    vk::Queue _queue;

  public:
    VulkanState() = default;

    vk::Instance instance() const { return _instance; }
    vk::PhysicalDevice phys_device() const { return _phys_device; }
    vk::Device device() const { return _device; }
    vk::RenderPass render_pass() const { return _render_pass; }
    vk::Queue queue() const { return _queue; }
  };
} // namespace ter

#endif // __vulkan_application_hpp_
