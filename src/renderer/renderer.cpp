#include "renderer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "window_context.hpp"

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
                                                     VkDebugUtilsMessageTypeFlagsEXT MessageType,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *CallbackData,
                                                     void *UserData )
{
  if (MessageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
  {
    // SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
    std::cout << "validation layer: " << CallbackData->pMessage << '\n' << std::endl;
    // SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
  }
  return VK_FALSE;
}

// ter::application class defualt constructor (initializes vulkan instance, device ...)
ter::Application::Application() 
{
  std::vector<const char *> extension_names;
  std::vector<const char *> layers_names;
  extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#ifdef WIN32
  extension_names.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
  extension_names.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

  if constexpr (1) 
  {
    extension_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    
    layers_names.push_back("VK_LAYER_KHRONOS_validation");
    layers_names.push_back("VK_LAYER_RENDERDOC_Capture");
  }

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

  // _instance = vk::createInstance(inst_ci);
  vk::Result result;
  std::tie(result, _state._instance) = vk::createInstance(inst_ci);
  assert(result == vk::Result::eSuccess);

  if constexpr (1)
  {
    VkDebugUtilsMessengerCreateInfoEXT debug_msgr_create_info {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};

    debug_msgr_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_msgr_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_msgr_create_info.pfnUserCallback = DebugCallback;

    auto create_debug_function = (PFN_vkCreateDebugUtilsMessengerEXT)_state._instance.getProcAddr("vkCreateDebugUtilsMessengerEXT");
    VkResult result;
    if (create_debug_function != nullptr)
      result = create_debug_function(_state._instance, &debug_msgr_create_info, nullptr, &_debug_messenger);

    assert(result == VK_SUCCESS);
  }

#if DEBUG
  debugUtilsMessenger = _instance.createDebugUtilsMessengerEXT(vk::su::makeDebugUtilsMessengerCreateInfoEXT());
#endif

  std::vector<vk::PhysicalDevice> phys_device_array;
  std::tie(result, phys_device_array) = _state._instance.enumeratePhysicalDevices();
  assert(result == vk::Result::eSuccess);
  _state._phys_device = phys_device_array.front();

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
  std::tie(result, _state._device) = _state._phys_device.createDevice(create_info);
  assert(result == vk::Result::eSuccess);

  assert(graphics_queue_family_index == 0);
  _state._queue = _state._device.getQueue(graphics_queue_family_index, 0);

  auto swapchain_format = vk::Format::eB8G8R8A8Unorm;
  vk::AttachmentDescription color_attachment
  {
    .format =  swapchain_format,
    .samples = vk::SampleCountFlagBits::e1,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
    .initialLayout = vk::ImageLayout::eUndefined,
    .finalLayout = vk::ImageLayout::ePresentSrcKHR,
  };

  vk::AttachmentReference color_attachment_ref { .attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal };

  vk::SubpassDescription subpass
  {
    .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment_ref,
  };

  vk::SubpassDependency dependency
  {
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
    .srcAccessMask = {},
    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
  };

  vk::RenderPassCreateInfo render_pass_create_info
  {
    .attachmentCount = 1,
    .pAttachments = &color_attachment,
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 1,
    .pDependencies = &dependency,   
  };

  std::tie(result, _state._render_pass) = _state._device.createRenderPass(render_pass_create_info);
  assert(result == vk::Result::eSuccess);
}

ter::Application::~Application() {}

[[nodiscard]] std::unique_ptr<ter::Buffer> ter::Application::create_buffer() const
{
  return std::make_unique<Buffer>();
}

[[nodiscard]] std::unique_ptr<ter::CommandUnit> ter::Application::create_command_unit() const
{
  return std::make_unique<CommandUnit>();
}

[[nodiscard]] std::unique_ptr<wnd::Window> ter::Application::create_window(size_t width, size_t height) const
{
  return std::make_unique<wnd::Window>(_state, width, height);
}
