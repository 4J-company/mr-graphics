#include "vulkan_state.hpp"
#include "utils/logic_guards.hpp"

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT MessageType,
  const VkDebugUtilsMessengerCallbackDataEXT *CallbackData, void *UserData)
{
  std::cerr << CallbackData->pMessage << "\n\n";
  return false;
}

// void mr::VulkanGlobalState::_init()
mr::VulkanGlobalState::VulkanGlobalState()
{
  while (vkfw::init() != vkfw::Result::eSuccess)
    ;

  std::vector<const char *> extension_names;
  std::vector<const char *> layers_names;
  extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#ifndef NDEBUG
  extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  layers_names.push_back("VK_LAYER_KHRONOS_validation");
#endif

  vk::ApplicationInfo app_info {.pApplicationName = "cgsg forever",
                                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                .pEngineName = "cgsg forever",
                                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                .apiVersion = VK_API_VERSION_1_3};

  uint32_t glfw_extensions_count = 0;
  auto glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);

  for (uint i = 0; i < glfw_extensions_count; i++) {
    extension_names.push_back(glfw_extensions[i]);
  }

  vk::InstanceCreateInfo inst_ci {
    .pApplicationInfo = &app_info,
    .enabledLayerCount = static_cast<uint>(layers_names.size()),
    .ppEnabledLayerNames = layers_names.data(),
    .enabledExtensionCount = static_cast<uint>(extension_names.size()),
    .ppEnabledExtensionNames = extension_names.data(),
  };

  _instance = vk::createInstance(inst_ci).value;

#ifndef NDEBUG
  auto create_debug_messenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
    _instance.getProcAddr("vkCreateDebugUtilsMessengerEXT")
  );
  assert(create_debug_messenger);

  const vk::DebugUtilsMessengerCreateInfoEXT debug_msgr_create_info {
    .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
    .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
    .pfnUserCallback = debug_callback,
  };

  create_debug_messenger(
    _instance,
    reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT *>(&debug_msgr_create_info),
    nullptr,
    reinterpret_cast<VkDebugUtilsMessengerEXT *>(&_debug_messenger)
  );
#endif

  _phys_device = _instance.enumeratePhysicalDevices().value.front();

  // TODO: create caches depending on program arguments for easier benchmarking
  _pipeline_cache.open(_cache_dir / "pipeline.cache");
  _validation_cache.open(_cache_dir / "validation.cache");
}

// void mr::VulkanGlobalState::_deinit()
mr::VulkanGlobalState::~VulkanGlobalState()
{
  if (_debug_messenger) {
    auto destroy_debug_messenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      _instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT")
    );
    assert(destroy_debug_messenger);
    destroy_debug_messenger(_instance, _debug_messenger, nullptr);
  }

  _instance.destroy();
}

mr::VulkanState::VulkanState(VulkanGlobalState *state)
  : _global(state)
{
  const std::vector<vk::QueueFamilyProperties> queue_family_properties =
    _global->_phys_device.getQueueFamilyProperties();

  size_t graphics_queue_family_index = -1;
  for (size_t i = 0; i < queue_family_properties.size(); i++) {
    if (queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) {
      graphics_queue_family_index = i;
    }
  }
  if (graphics_queue_family_index == -1) {
    std::exit(1);
  }

  const float queue_priority = 1.0f;
  const vk::DeviceQueueCreateInfo device_queue_ci {
    .flags = {},
    .queueFamilyIndex = static_cast<uint32_t>(graphics_queue_family_index),
    .queueCount = 1,
    .pQueuePriorities = &queue_priority
  };

  std::vector<const char *> extension_names {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_VALIDATION_CACHE_EXTENSION_NAME,
  };

  const auto supported_features = _global->_phys_device.getFeatures();

  const vk::PhysicalDeviceFeatures required_features {
    .geometryShader = true,
    .tessellationShader = true,
    .multiDrawIndirect = supported_features.multiDrawIndirect,
    .samplerAnisotropy = true,
  };

  const vk::DeviceCreateInfo create_info {
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &device_queue_ci,
    .enabledLayerCount = 0,
    .enabledExtensionCount = static_cast<uint>(extension_names.size()),
    .ppEnabledExtensionNames = extension_names.data(),
    .pEnabledFeatures = &required_features,
  };
  _device = _global->_phys_device.createDeviceUnique(create_info).value;
  _queue = _device->getQueue(graphics_queue_family_index, 0);

  _create_pipeline_cache();
  _create_validation_cache();
}

mr::VulkanState::~VulkanState()
{
  _destroy_pipeline_cache();
  _destroy_validation_cache();
}

void mr::VulkanState::_create_pipeline_cache()
{
  const auto &cache_bytes = _global->_pipeline_cache.bytes();
  vk::PipelineCacheCreateInfo create_info {
    .initialDataSize = cache_bytes.size(),
    .pInitialData = cache_bytes.data()
  };
  _pipeline_cache = _device->createPipelineCacheUnique(create_info).value;
}

void mr::VulkanState::_destroy_pipeline_cache()
{
  if (not _pipeline_cache) {
    return;
  }

  size_t cache_size = 0;
  vk::Result result = _device->getPipelineCacheData(*_pipeline_cache, &cache_size, nullptr);
  if (result != vk::Result::eSuccess) {
    return;
  }

  auto &cache_bytes = _global->_pipeline_cache.bytes();
  cache_bytes.resize(cache_size);
  result = _device->getPipelineCacheData(*_pipeline_cache, &cache_size, cache_bytes.data());
  if (result != vk::Result::eSuccess) {
    return;
  }
}

void mr::VulkanState::_create_validation_cache()
{
  // TODO: search for an easier way to load functions (volk?)
  auto create_cache = reinterpret_cast<PFN_vkCreateValidationCacheEXT>(
    _global->_instance.getProcAddr("vkCreateValidationCacheEXT")
  );
  if (create_cache == nullptr) {
    return;
  }

  const auto &cache_bytes = _global->_pipeline_cache.bytes();
  const vk::ValidationCacheCreateInfoEXT create_info {
    .initialDataSize = cache_bytes.size(),
    .pInitialData = cache_bytes.data()
  };

  create_cache(
    *_device,
    reinterpret_cast<const VkValidationCacheCreateInfoEXT *>(&create_info),
    nullptr,
    reinterpret_cast<VkValidationCacheEXT *>(&_validation_cache)
  );
}

void mr::VulkanState::_destroy_validation_cache()
{
  if (not _device || not _validation_cache) {
    return;
  }

  on_scope_exit {
    auto destroy_cache = reinterpret_cast<PFN_vkDestroyValidationCacheEXT>(
      _global->_instance.getProcAddr("vkDestroyValidationCacheEXT")
    );
    if (destroy_cache != nullptr) {
      destroy_cache(*_device, _validation_cache, nullptr);
    }
  };

  auto get_cache_data = reinterpret_cast<PFN_vkGetValidationCacheDataEXT>(
    _global->_instance.getProcAddr("vkGetValidationCacheDataEXT")
  );
  if (get_cache_data == nullptr) {
    return;
  }

  size_t cache_size = 0;
  VkResult result = get_cache_data(*_device, _validation_cache, &cache_size, nullptr);
  if (result != VK_SUCCESS) {
    return;
  }

  auto &cache_bytes = _global->_validation_cache.bytes();
  cache_bytes.resize(cache_size);
  result = get_cache_data(*_device, _validation_cache, &cache_size, cache_bytes.data());
  if (result != VK_SUCCESS) {
    return;
  }
}
