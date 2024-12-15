#include "vulkan_state.hpp"
#include "utils/logic_guards.hpp"

mr::VulkanGlobalState::VulkanGlobalState()
{
  while (vkfw::init() != vkfw::Result::eSuccess)
    ;

  _create_instance();
  _create_phys_device();

  // TODO: create caches depending on program arguments for easier benchmarking
  _pipeline_cache.open(_cache_dir / "pipeline.cache");
  _validation_cache.open(_cache_dir / "validation.cache");
}

mr::VulkanGlobalState::~VulkanGlobalState()
{
  vkb::destroy_instance(_instance);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
  VkDebugUtilsMessageTypeFlagsEXT message_type,
  const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data)
{
  const auto severity = vkb::to_string_message_severity(message_severity);
  const auto type = vkb::to_string_message_type(message_type);
  std::cerr << callback_data->messageIdNumber << ' ' << type << ' ' << severity
    << ": " << callback_data->pMessage << "\n\n";
  return false;
}

void mr::VulkanGlobalState::_create_instance()
{
  vkb::InstanceBuilder builder;
  builder
    .set_app_name("cgsg forever")
    .set_app_version(VK_MAKE_VERSION(1, 0, 0))
    .set_engine_name("cgsg forever")
    .set_engine_version(VK_MAKE_VERSION(1, 0, 0))
    .require_api_version(VK_API_VERSION_1_3)
    .enable_extensions({VK_KHR_SURFACE_EXTENSION_NAME});

  uint32_t glfw_extensions_count = 0;
  const auto glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);
  for (uint i = 0; i < glfw_extensions_count; i++) {
    builder.enable_extension(glfw_extensions[i]);
  }

#ifndef NDEBUG
  const auto sys_info = vkb::SystemInfo::get_system_info().value();
  if (sys_info.debug_utils_available) {
    builder.enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  builder.enable_validation_layers(sys_info.validation_layers_available);

  builder
    .add_debug_messenger_severity(
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    .add_debug_messenger_type(
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    .set_debug_callback(debug_callback);
#endif

  const auto instance = builder.build();
  if (not instance) {
    std::cerr << "Cannot create VkInstance: " << instance.error().message() << "\n";
  }
  _instance = instance.value();
}

void mr::VulkanGlobalState::_create_phys_device()
{
  vkb::PhysicalDeviceSelector selector{_instance};
  const auto phys_device = selector
    .defer_surface_initialization()
    .add_required_extensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME})
    .select();
  if (not phys_device) {
    std::cerr << "Cannot create VkPhysicalDevice: " << phys_device.error().message() << "\n";
  }
  _phys_device = phys_device.value();

  _phys_device.enable_extensions_if_present({VK_EXT_VALIDATION_CACHE_EXTENSION_NAME});

  const vk::PhysicalDeviceFeatures required_features {
    .geometryShader = true,
    .tessellationShader = true,
    .samplerAnisotropy = true,
  };
  if (not _phys_device.enable_features_if_present(static_cast<const VkPhysicalDeviceFeatures &>(required_features))) {
    std::cerr << "VkPhysicalDevice does not support required features\n";
  }

  const vk::PhysicalDeviceFeatures optional_features {
    .multiDrawIndirect = true,
  };
  _phys_device.enable_features_if_present(static_cast<const VkPhysicalDeviceFeatures &>(optional_features));
}

mr::VulkanState::VulkanState(VulkanGlobalState *state)
  : _global(state)
  , _device({}, {nullptr})
{
  _create_device();
  _create_pipeline_cache();
  _create_validation_cache();
}

mr::VulkanState::~VulkanState()
{
  _destroy_pipeline_cache();
  _destroy_validation_cache();
  //_device.destroy();
}

void mr::VulkanState::_create_device()
{
  std::vector<vkb::CustomQueueDescription> queue_descrs;
  auto queue_families = _global->_phys_device.get_queue_families ();
  for (size_t i = 0; i < queue_families.size(); i++) {
    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      queue_descrs.emplace_back(static_cast<uint>(i), std::vector(queue_families[i].queueCount, 1.0f));
      break;
    }
  }
  vkb::DeviceBuilder builder{_global->_phys_device};
  auto device = builder.custom_queue_setup(queue_descrs).build();
  if (not device) {
    std::cerr << "Cannot create VkDevice: " << device.error().message() << "\n";
  }
  _device.reset(device.value().device);

  auto queue = device.value().get_queue(vkb::QueueType::graphics);
  if (not queue) {
    std::cerr << "Cannot create VkQueue: " << queue.error().message() << "\n";
  }
  _queue = queue.value();
}

void mr::VulkanState::_create_pipeline_cache()
{
  const auto &cache_bytes = _global->_pipeline_cache.bytes();
  vk::PipelineCacheCreateInfo create_info {
    .initialDataSize = cache_bytes.size(),
    .pInitialData = cache_bytes.data()
  };
  _pipeline_cache = device().createPipelineCacheUnique(create_info).value;
}

void mr::VulkanState::_destroy_pipeline_cache()
{
  if (not _pipeline_cache) {
    return;
  }

  size_t cache_size = 0;
  vk::Result result = device().getPipelineCacheData(pipeline_cache(), &cache_size, nullptr);
  if (result != vk::Result::eSuccess) {
    return;
  }

  auto &cache_bytes = _global->_pipeline_cache.bytes();
  cache_bytes.resize(cache_size);
  result = device().getPipelineCacheData(pipeline_cache(), &cache_size, cache_bytes.data());
  if (result != vk::Result::eSuccess) {
    return;
  }
}

void mr::VulkanState::_create_validation_cache()
{
  if (not _global->_phys_device.is_extension_present(VK_EXT_VALIDATION_CACHE_EXTENSION_NAME)) {
    return;
  }

  // TODO: search for an easier way to load functions (volk?)
  auto create_cache = reinterpret_cast<PFN_vkCreateValidationCacheEXT>(
    instance().getProcAddr("vkCreateValidationCacheEXT")
  );
  if (create_cache == nullptr) {
    return;
  }

  const auto &cache_bytes = _global->_validation_cache.bytes();
  const vk::ValidationCacheCreateInfoEXT create_info {
    .initialDataSize = cache_bytes.size(),
    .pInitialData = cache_bytes.data()
  };

  create_cache(
    device(),
    reinterpret_cast<const VkValidationCacheCreateInfoEXT *>(&create_info),
    nullptr,
    reinterpret_cast<VkValidationCacheEXT *>(&_validation_cache)
  );
}

void mr::VulkanState::_destroy_validation_cache()
{
  if (not _validation_cache) {
    return;
  }

  on_scope_exit {
    auto destroy_cache = reinterpret_cast<PFN_vkDestroyValidationCacheEXT>(
      instance().getProcAddr("vkDestroyValidationCacheEXT")
    );
    if (destroy_cache != nullptr) {
      destroy_cache(*_device, _validation_cache, nullptr);
    }
  };

  auto get_cache_data = reinterpret_cast<PFN_vkGetValidationCacheDataEXT>(
    instance().getProcAddr("vkGetValidationCacheDataEXT")
  );
  if (get_cache_data == nullptr) {
    return;
  }

  size_t cache_size = 0;
  VkResult result = get_cache_data(device(), _validation_cache, &cache_size, nullptr);
  if (result != VK_SUCCESS) {
    return;
  }

  auto &cache_bytes = _global->_validation_cache.bytes();
  cache_bytes.resize(cache_size);
  result = get_cache_data(device(), _validation_cache, &cache_size, cache_bytes.data());
  if (result != VK_SUCCESS) {
    return;
  }
}
