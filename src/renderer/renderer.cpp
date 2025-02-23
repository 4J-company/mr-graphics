#include "renderer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "window/render_context.hpp"
#include "utils/log.hpp"
#include "manager/manager.hpp"

/**
 * @brief Handles Vulkan debug messages by logging them based on severity.
 *
 * This callback interprets the severity of debug messages from Vulkan and logs the
 * message accordingly. Errors are logged as errors, warnings as warnings, and all other
 * messages as informational. The function always returns VK_FALSE to ensure that the 
 * message is not discarded.
 *
 * @param MessageSeverity The severity level of the debug message.
 * @param MessageType The type flags of the debug message.
 * @param CallbackData Pointer to the callback data containing the debug message.
 * @param UserData Optional user data provided during callback setup.
 * @return VkBool32 Always returns VK_FALSE.
 */
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT MessageType,
  const VkDebugUtilsMessengerCallbackDataEXT *CallbackData, void *UserData)
{
  switch (MessageSeverity) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    MR_ERROR("{}\n", CallbackData->pMessage);
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    MR_WARNING("{}\n", CallbackData->pMessage);
    break;
  default:
    MR_INFO("{}\n", CallbackData->pMessage);
    break;
  }
  return false;
}

/**
 * @brief Constructs an Application instance and initializes the Vulkan framework.
 *
 * This constructor repeatedly attempts to initialize Vulkan by calling vkfw::init() until a successful
 * initialization is achieved, ensuring that the Vulkan instance and device are ready for subsequent operations.
 */
mr::Application::Application()
{
  while (vkfw::init() != vkfw::Result::eSuccess)
    ;
  // _state._init();
}

/**
 * @brief Destroys the Application instance.
 *
 * This destructor is intended to handle resource cleanup for the Application. Currently,
 * the cleanup operation (deinitialization of internal state) is commented out, indicating that
 * resource management might be enhanced in a future update.
 */
mr::Application::~Application()
{
  // _state._deinit();
}

[[nodiscard]] std::unique_ptr<mr::HostBuffer>
mr::Application::create_host_buffer() const
{
  return std::make_unique<HostBuffer>();
}

[[nodiscard]] std::unique_ptr<mr::DeviceBuffer>
mr::Application::create_device_buffer() const
{
  return std::make_unique<DeviceBuffer>();
}

[[nodiscard]] std::unique_ptr<mr::CommandUnit>
mr::Application::create_command_unit() const
{
  return std::make_unique<CommandUnit>();
}

/**
 * @brief Creates a new window instance.
 *
 * This method instantiates a Window object using the provided extent and the internal application state.
 * The window is returned as a unique pointer for exclusive ownership.
 *
 * @param extent The dimensions and other parameters defining the window's properties.
 * @return std::unique_ptr<mr::Window> A unique pointer to the created Window object.
 */
[[nodiscard]] std::unique_ptr<mr::Window>
mr::Application::create_window(Extent extent)
{
  return std::make_unique<Window>(&_state, extent);
}
