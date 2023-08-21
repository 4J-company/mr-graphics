#if !defined(__vulkan_application_hpp_)
#define __vulkan_application_hpp_

#include "pch.hpp"

namespace ter
{
  class VulkanApplication 
  {
    friend class WindowContext;

  private:
  protected:
  public:
    vk::Instance _instance;
    vk::Device _device;
    vk::PhysicalDevice _phys_device;

    vk::Queue _queue;
#if DEBUG
    vk::DebugUtilsMessengerEXT _dbg_messenger;
#endif

    vk::RenderPass _render_pass;

    VulkanApplication();
    ~VulkanApplication();

    void create_render_pass(vk::Format swapchain_format);

  public:
    const vk::Device & get_device() { return _device; }
    const vk::RenderPass & get_render_pass() { return _render_pass; }
    const vk::Instance & get_instance() { return _instance; }

  private:
    VkDebugUtilsMessengerEXT DebugMessenger;
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
                                                          VkDebugUtilsMessageTypeFlagsEXT MessageType,
                                                          const VkDebugUtilsMessengerCallbackDataEXT *CallbackData,
                                                          void *UserData );
  };
}

#endif // __vulkan_application_hpp_