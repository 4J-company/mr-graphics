#include "vulkan_state.hpp"
#include <vulkan/vulkan_core.h>

mr::VulkanGlobalState::VulkanGlobalState()
{
  while (vkfw::init() != vkfw::Result::eSuccess) {}

  _create_instance();
  _create_phys_device();

  // TODO: create caches depending on program arguments for easier benchmarking
  std::fs::path pipeline_cache_path = path::cache_dir / "pipeline.cache";
  MR_INFO("Reading pipeline cache from {}", pipeline_cache_path.string());
  _pipeline_cache_file.open_or_create(std::move(pipeline_cache_path));
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
  switch (message_severity) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    MR_ERROR("{}\n", callback_data->pMessage);
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    MR_WARNING("{}\n", callback_data->pMessage);
    break;
  default:
    MR_INFO("{}\n", callback_data->pMessage);
    break;
  }
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
    MR_ERROR("Cannot create VkInstance. {}\n", instance.error().message());
  }
  _instance = instance.value();
}

void mr::VulkanGlobalState::_create_phys_device()
{
  vk::PhysicalDeviceFeatures features {
    .geometryShader = true,
    .tessellationShader = true,
    .multiDrawIndirect = true,
    .samplerAnisotropy = true,
  };

  vk::PhysicalDeviceVulkan12Features features12{
    .descriptorIndexing = true,
    .bufferDeviceAddress = true,
  };

  vk::PhysicalDeviceVulkan13Features features13{
    .synchronization2 = true,
    .dynamicRendering = true,
  };

  vk::PhysicalDeviceVulkan14Features features14 {
    .dynamicRenderingLocalRead = true,
  };

  vkb::PhysicalDeviceSelector selector{_instance};
  const auto phys_device = selector
    .set_minimum_version(1, 3)
    .defer_surface_initialization()
    .set_required_features(features)
    .set_required_features_12(features12)
    .set_required_features_13(features13)
    .set_required_features_14(features14)
    .add_required_extensions({
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME})
    .select();

  if (not phys_device) {
    MR_ERROR("Cannot create VkPhysicalDevice. {}\n", phys_device.error().message());
  }
  _phys_device = phys_device.value();

  _phys_device.enable_extensions_if_present({VK_EXT_VALIDATION_CACHE_EXTENSION_NAME});
}

mr::VulkanState::VulkanState(VulkanGlobalState *state)
  : _global(state)
  , _device({}, {nullptr})
{
  _create_device();
  _create_pipeline_cache();
}

mr::VulkanState::~VulkanState()
{
  _destroy_pipeline_cache();
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
    MR_ERROR("Cannot create VkDevice. {}\n", device.error().message());
  }
  _device.reset(device.value().device);

  auto queue = device.value().get_queue(vkb::QueueType::graphics);
  if (not queue) {
    MR_ERROR("Cannot create VkQueue {}\n ", queue.error().message());
  }
  _queue = queue.value();
}

void mr::VulkanState::_create_pipeline_cache()
{
  const auto &cache_bytes = _global->_pipeline_cache_file.bytes();
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

  auto &cache_bytes = _global->_pipeline_cache_file.bytes();
  cache_bytes.resize(cache_size);
  result = device().getPipelineCacheData(pipeline_cache(), &cache_size, cache_bytes.data());
  if (result != vk::Result::eSuccess) {
    return;
  }
}
