#include "renderer.hpp"
#include <GLFW/glfw3.h>

// ter::application class defualt constructor (initializes vulkan instance,
// device ...)
ter::application::application() {
  vk::ApplicationInfo app_info{/* name */ "First triangle",
                               /* version */ VK_MAKE_VERSION(1, 0, 0),
                               /* engine name */ "TER Engine",
                               /* engine verison */ VK_MAKE_VERSION(1, 0, 0),
                               /* vulkan version */ VK_API_VERSION_1_1};
  vk::InstanceCreateInfo inst_ci{{}, &app_info};

  _instance = vk::createInstance(inst_ci);

#if __DEBUG
  debugUtilsMessenger = _instance.createDebugUtilsMessengerEXT(
      vk::su::makeDebugUtilsMessengerCreateInfoEXT());
#endif

  _phys_device = _instance.enumeratePhysicalDevices().front();

  // get the QueueFamilyProperties of the first PhysicalDevice
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
      _phys_device.getQueueFamilyProperties();

  // get the first index into queueFamiliyProperties which supports graphics
  auto propertyIterator =
      std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
                   [](vk::QueueFamilyProperties const &qfp) {
                     return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
                   });
  size_t graphicsQueueFamilyIndex =
      std::distance(queueFamilyProperties.begin(), propertyIterator);
  assert(graphicsQueueFamilyIndex < queueFamilyProperties.size());

  // create a Device
  float queuePriority = 0.0f;
  vk::DeviceQueueCreateInfo device_queue_ci(
      vk::DeviceQueueCreateFlags(),
      static_cast<uint32_t>(graphicsQueueFamilyIndex), 1, &queuePriority);
  _device = _phys_device.createDevice(
      vk::DeviceCreateInfo(vk::DeviceCreateFlags(), device_queue_ci));

  // define extensions
  // create physical device
  // create logical device
  // create queues (multiple ?)
} // end of 'ter::application::application' function

ter::application::~application() {
  _device.destroy();
#if __DEBUG
  _instance.destroyDebugUtilsMessengerEXT(_dbg_messenger);
#endif
  _instance.destroy();
}

std::unique_ptr<ter::command_unit> ter::application::create_command_unit() {
  return std::make_unique<command_unit>();
}
