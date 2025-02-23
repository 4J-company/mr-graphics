#include "vulkan_state.hpp"
#include "utils/logic_guards.hpp"
#include "utils/path.hpp"

/**
 * @brief Constructs and initializes the Vulkan global state.
 *
 * This constructor repeatedly calls `vkfw::init()` until the Vulkan framework is successfully initialized.
 * It then creates the Vulkan instance and selects a physical device by invoking `_create_instance()` and 
 * `_create_phys_device()`, respectively. Finally, it sets up the pipeline cache by constructing its file path 
 * using the cache directory and initializing it with `open_or_create`, logging the cache location for reference.
 *
 * @note Future enhancements may adjust cache creation based on program arguments for benchmarking purposes.
 */
mr::VulkanGlobalState::VulkanGlobalState()
{
  while (vkfw::init() != vkfw::Result::eSuccess)
    ;

  _create_instance();
  _create_phys_device();

  // TODO: create caches depending on program arguments for easier benchmarking
  std::fs::path pipeline_cache_path = path::cache_dir / "pipeline.cache";
  MR_INFO("Reading pipeline cache from {}", pipeline_cache_path.string());
  _pipeline_cache.open_or_create(std::move(pipeline_cache_path));
}

/**
 * @brief Destructor for VulkanGlobalState.
 *
 * Releases the Vulkan instance by calling vkb::destroy_instance, ensuring that all associated
 * Vulkan resources are properly cleaned up.
 */
mr::VulkanGlobalState::~VulkanGlobalState()
{
  vkb::destroy_instance(_instance);
}

/**
 * @brief Callback for logging Vulkan debug messages.
 *
 * This function processes Vulkan debug messages and logs them based on their severity.
 * Error messages are logged using MR_ERROR, warnings with MR_WARNING, and less severe messages
 * with MR_INFO. It always returns VK_FALSE to indicate that the callback does not interrupt execution.
 *
 * @param message_severity The severity level of the debug message.
 * @param message_type The type of the debug message.
 * @param callback_data Pointer containing details about the debug message.
 * @param user_data Unused user-defined data.
 *
 * @return VK_FALSE always.
 */
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

/**
 * @brief Creates and configures the Vulkan instance.
 *
 * Initializes a Vulkan instance using a builder that sets the application and engine details,
 * requires Vulkan API version 1.3, and enables necessary extensions including GLFW surface extensions.
 * In debug builds, it also enables debug utilities and validation layers, and sets up a debug callback
 * for enhanced error reporting. Logs an error if the instance creation fails.
 */
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

/**
 * @brief Selects and configures the Vulkan physical device.
 *
 * Uses a device selector to choose a physical device that supports the swapchain extension while deferring
 * surface initialization. If no device is selected, an error is logged. Once a device is chosen, the function
 * conditionally enables the VK_EXT_VALIDATION_CACHE_EXTENSION_NAME extension, attempts to enable essential features
 * (geometry shader, tessellation shader, sampler anisotropy), and logs an error if these are not available.
 * It also attempts to enable the optional multi-draw indirect feature.
 */
void mr::VulkanGlobalState::_create_phys_device()
{
  vkb::PhysicalDeviceSelector selector{_instance};
  const auto phys_device = selector
    .defer_surface_initialization()
    .add_required_extensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME})
    .select();
  if (not phys_device) {
    MR_ERROR("Cannot create VkPhysicalDevice. {}\n", phys_device.error().message());
  }
  _phys_device = phys_device.value();

  _phys_device.enable_extensions_if_present({VK_EXT_VALIDATION_CACHE_EXTENSION_NAME});

  const vk::PhysicalDeviceFeatures required_features {
    .geometryShader = true,
    .tessellationShader = true,
    .samplerAnisotropy = true,
  };
  if (not _phys_device.enable_features_if_present(static_cast<const VkPhysicalDeviceFeatures &>(required_features))) {
    MR_ERROR("VkPhysicalDevice does not support required features\n");
  }

  const vk::PhysicalDeviceFeatures optional_features {
    .multiDrawIndirect = true,
  };
  _phys_device.enable_features_if_present(static_cast<const VkPhysicalDeviceFeatures &>(optional_features));
}

/**
 * @brief Constructs a VulkanState instance.
 *
 * Initializes device resources and sets up a pipeline cache using the provided global Vulkan state.
 * This prepares the VulkanState for subsequent device operations.
 *
 * @param state Pointer to the global Vulkan state containing the Vulkan context.
 */
mr::VulkanState::VulkanState(VulkanGlobalState *state)
  : _global(state)
  , _device({}, {nullptr})
{
  _create_device();
  _create_pipeline_cache();
}

/**
 * @brief Destroys the Vulkan state and releases associated resources.
 *
 * This destructor ensures that the Vulkan pipeline cache is properly cleaned up by invoking _destroy_pipeline_cache().
 */
mr::VulkanState::~VulkanState()
{
  _destroy_pipeline_cache();
}

/**
 * @brief Creates a Vulkan device and initializes the graphics queue.
 *
 * This method selects a graphics-supporting queue family from the global physical device and constructs
 * a Vulkan device with a custom queue configuration. Upon successful creation, it sets the device
 * and retrieves the corresponding graphics queue. Error messages are logged if the device or queue 
 * cannot be created.
 */
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
  const auto &cache_bytes = _global->_pipeline_cache.bytes();
  vk::PipelineCacheCreateInfo create_info {
    .initialDataSize = cache_bytes.size(),
    .pInitialData = cache_bytes.data()
  };
  _pipeline_cache = device().createPipelineCacheUnique(create_info).value;
}

/**
 * @brief Retrieves and stores pipeline cache data for later reuse.
 *
 * If a valid pipeline cache exists, this function queries its size using the Vulkan device,
 * resizes the global pipeline cache's data buffer accordingly, and retrieves the cache data.
 * The operation is aborted silently if the pipeline cache is not set or if any Vulkan call fails.
 */
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
