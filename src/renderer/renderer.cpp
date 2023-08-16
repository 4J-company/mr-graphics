#include "renderer.hpp"

// ter::application class defualt constructor (initializes vulkan instance, device ...)
ter::Application::Application()
{
  vk::ApplicationInfo app_info {.pApplicationName = "cgsg forever",
                                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                .pEngineName = "cgsg forever",
                                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                .apiVersion = VK_API_VERSION_1_3};
  vk::InstanceCreateInfo inst_ci {{}, &app_info};

  _instance = vk::createInstance(inst_ci);

#if DEBUG
  debugUtilsMessenger = _instance.createDebugUtilsMessengerEXT(vk::su::makeDebugUtilsMessengerCreateInfoEXT());
#endif

  _phys_device = _instance.enumeratePhysicalDevices().front();

  // get the QueueFamilyProperties of the first PhysicalDevice
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties = _phys_device.getQueueFamilyProperties();

  // get the first index into queueFamiliyProperties which supports graphics
  auto property_iterator =
      std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
                   [](vk::QueueFamilyProperties const &qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; });
  size_t graphics_queue_family_index = std::distance(queueFamilyProperties.begin(), property_iterator);
  assert(graphics_queue_family_index < queueFamilyProperties.size());

  // create a Device
  float queuePriority = 0.0f;
  vk::DeviceQueueCreateInfo device_queue_ci {.flags = {},
                                             .queueFamilyIndex = static_cast<uint32_t>(graphics_queue_family_index),
                                             .queueCount = 1,
                                             .pQueuePriorities = &queuePriority};
} // End of 'ter::application::application' function

ter::Application::~Application()
{
  _device.destroy();
#if DEBUG
  _instance.destroyDebugUtilsMessengerEXT(_dbg_messenger);
#endif
  _instance.destroy();
}

std::unique_ptr<ter::Buffer> ter::Application::create_buffer() { return std::make_unique<Buffer>(); }
std::unique_ptr<ter::CommandUnit> ter::Application::create_command_unit() { return std::make_unique<CommandUnit>(); }
