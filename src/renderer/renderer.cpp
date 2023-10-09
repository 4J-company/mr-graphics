#include "renderer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "window_context.hpp"

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
                                                     VkDebugUtilsMessageTypeFlagsEXT MessageType,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *CallbackData,
                                                     void *UserData )
{
  std::cout << CallbackData->pMessage << '\n' << std::endl;
  return false;
}

// mr::Application class defualt constructor (initializes vulkan instance, device ...)
mr::Application::Application() 
{
  std::vector<const char *> extension_names;
  std::vector<const char *> layers_names;
  extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
  extension_names.push_back(VK_XWIN_SURFACE_EXTENSION_NAME);

#if !NDEBUG 
  extension_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  layers_names.push_back("VK_LAYER_KHRONOS_validation");
  layers_names.push_back("VK_LAYER_RENDERDOC_Capture");
#endif

  vk::ApplicationInfo app_info 
  {
    .pApplicationName = "cgsg forever",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "cgsg forever",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_3
  };
  vk::InstanceCreateInfo inst_ci
  {
    .pApplicationInfo = &app_info,
    .enabledLayerCount = static_cast<uint>(layers_names.size()),
    .ppEnabledLayerNames = layers_names.data(),
    .enabledExtensionCount = static_cast<uint>(extension_names.size()),
    .ppEnabledExtensionNames = extension_names.data(),
  };

  _state._instance = vk::createInstance(inst_ci).value;

#if !NDEBUG
  auto pfnVkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>( _state.instance().getProcAddr( "vkCreateDebugUtilsMessengerEXT" ) );
  assert(pfnVkCreateDebugUtilsMessengerEXT);

  auto pfnVkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>( _state.instance().getProcAddr( "vkDestroyDebugUtilsMessengerEXT" ) );
  assert(pfnVkDestroyDebugUtilsMessengerEXT);

  vk::DebugUtilsMessengerCreateInfoEXT debug_msgr_create_info 
  {
    .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
    .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
    .pfnUserCallback = DebugCallback,
  };

  pfnVkCreateDebugUtilsMessengerEXT(_state.instance(), reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT *>(&debug_msgr_create_info), nullptr, &_debug_messenger);
#endif

  _state._phys_device = _state._instance.enumeratePhysicalDevices().value.front();

  // get the QueueFamilyProperties of the first PhysicalDevice
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties = _state._phys_device.getQueueFamilyProperties();

  // get the first index into queueFamiliyProperties which supports graphics
  auto property_iterator =
      std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
                   [](vk::QueueFamilyProperties const &qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; });
  size_t graphics_queue_family_index = std::distance(queueFamilyProperties.begin(), property_iterator);
  assert(graphics_queue_family_index < queueFamilyProperties.size());

  // create a Device
  float queue_priority = 1.0f;
  vk::DeviceQueueCreateInfo device_queue_ci
  {
    .flags = {},
    .queueFamilyIndex = static_cast<uint32_t>(graphics_queue_family_index),
    .queueCount = 1,
    .pQueuePriorities = &queue_priority
  };

  extension_names.clear();
  extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  auto supported_features = _state._phys_device.getFeatures();

  vk::PhysicalDeviceFeatures required_features 
  {
    .geometryShader = true,
    .tessellationShader = true,
    .multiDrawIndirect = supported_features.multiDrawIndirect,
    .samplerAnisotropy = true,
  };

  vk::DeviceCreateInfo create_info
  {
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &device_queue_ci,
    .enabledLayerCount = 0,
    .enabledExtensionCount = static_cast<uint>(extension_names.size()),
    .ppEnabledExtensionNames = extension_names.data(),
    .pEnabledFeatures = &required_features,
  };
  _state._device = _state._phys_device.createDevice(create_info).value;

  assert(graphics_queue_family_index == 0);
  _state._queue = _state._device.getQueue(graphics_queue_family_index, 0);

}

mr::Application::~Application() 
{
  _state._device.destroy();
  _state._instance.destroy();
}

[[nodiscard]] std::unique_ptr<mr::Buffer> mr::Application::create_buffer() const { return std::make_unique<Buffer>(); }

[[nodiscard]] std::unique_ptr<mr::CommandUnit> mr::Application::create_command_unit() const
{
  return std::make_unique<CommandUnit>();
}

[[nodiscard]] std::unique_ptr<mr::Window> mr::Application::create_window(size_t width, size_t height) const
{
  return std::make_unique<Window>(_state, width, height);
}

[[nodiscard]] mr::Mesh * mr::Application::create_mesh(
    std::span<PositionType> positions,
    std::span<FaceType> faces,
    std::span<ColorType> colors,
    std::span<TexCoordType> uvs,
    std::span<NormalType> normals,
    std::span<NormalType> tangents,
    std::span<NormalType> bitangent,
    std::span<BoneType> bones,
    BoundboxType bbox
    ) const {
  _tmp_mesh_pool.emplace_back(positions, faces, colors, uvs, normals, tangents, bitangent, bones, bbox);
  return &_tmp_mesh_pool.back();
}
