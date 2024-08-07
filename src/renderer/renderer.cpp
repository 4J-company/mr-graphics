#include "renderer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "window/window_context.hpp"

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT MessageType,
  const VkDebugUtilsMessengerCallbackDataEXT *CallbackData, void *UserData)
{
  std::cout << CallbackData->pMessage << '\n' << std::endl;
  return false;
}

// mr::Application class defualt constructor (initializes vulkan instance, device ...)
mr::Application::Application()
{
  while (vkfw::init() != vkfw::Result::eSuccess)
    ;

  std::vector<const char *> extension_names;
  std::vector<const char *> layers_names;
  extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#ifndef NDEBUG
  extension_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  layers_names.push_back("VK_LAYER_KHRONOS_validation");
#endif

  vk::ApplicationInfo app_info {.pApplicationName = "cgsg forever",
                                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                .pEngineName = "cgsg forever",
                                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                .apiVersion = VK_API_VERSION_1_3};

  uint32_t amountOfGlfwExtensions = 0;
  auto glfwExtensions =
    glfwGetRequiredInstanceExtensions(&amountOfGlfwExtensions);

  for (uint i = 0; i < amountOfGlfwExtensions; i++) {
    extension_names.push_back(glfwExtensions[i]);
  }

  vk::InstanceCreateInfo inst_ci {
    .pApplicationInfo = &app_info,
    .enabledLayerCount = static_cast<uint>(layers_names.size()),
    .ppEnabledLayerNames = layers_names.data(),
    .enabledExtensionCount = static_cast<uint>(extension_names.size()),
    .ppEnabledExtensionNames = extension_names.data(),
  };

  _state._instance = vk::createInstance(inst_ci).value;

#ifndef NDEBUG
  auto pfnVkCreateDebugUtilsMessengerEXT =
    reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      _state.instance().getProcAddr("vkCreateDebugUtilsMessengerEXT"));
  assert(pfnVkCreateDebugUtilsMessengerEXT);

  auto pfnVkDestroyDebugUtilsMessengerEXT =
    reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      _state.instance().getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
  assert(pfnVkDestroyDebugUtilsMessengerEXT);

  vk::DebugUtilsMessengerCreateInfoEXT debug_msgr_create_info {
    .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
    .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
    .pfnUserCallback = DebugCallback,
  };

  pfnVkCreateDebugUtilsMessengerEXT(
    _state.instance(),
    reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT *>(
      &debug_msgr_create_info),
    nullptr,
    &_debug_messenger);
#endif

  _state._phys_device =
    _state._instance.enumeratePhysicalDevices().value.front();

  // get the QueueFamilyProperties of the first PhysicalDevice
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
    _state._phys_device.getQueueFamilyProperties();

  // get the first index into queueFamiliyProperties which supports graphics
  auto property_iterator =
    std::find_if(queueFamilyProperties.begin(),
                 queueFamilyProperties.end(),
                 [](const vk::QueueFamilyProperties &qfp) {
                   return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
                 });
  size_t graphics_queue_family_index =
    std::distance(queueFamilyProperties.begin(), property_iterator);
  assert(graphics_queue_family_index < queueFamilyProperties.size());

  // create a Device
  float queue_priority = 1.0f;
  vk::DeviceQueueCreateInfo device_queue_ci {
    .flags = {},
    .queueFamilyIndex = static_cast<uint32_t>(graphics_queue_family_index),
    .queueCount = 1,
    .pQueuePriorities = &queue_priority};

  extension_names.clear();
  extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  auto supported_features = _state._phys_device.getFeatures();

  vk::PhysicalDeviceFeatures required_features {
    .geometryShader = true,
    .tessellationShader = true,
    .multiDrawIndirect = supported_features.multiDrawIndirect,
    .samplerAnisotropy = true,
  };

  vk::DeviceCreateInfo create_info {
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

  /*
  auto swapchain_format = vk::Format::eB8G8R8A8Unorm;
  vk::AttachmentDescription color_attachment
  {
    .format = swapchain_format,
      .samples = vk::SampleCountFlagBits::e1,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
      .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
      .initialLayout = vk::ImageLayout::eUndefined,
      .finalLayout = vk::ImageLayout::ePresentSrcKHR,
  };

  vk::AttachmentReference color_attachment_ref {.attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal };

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

  _state._render_pass = _state._device.createRenderPass(render_pass_create_info).value;
  */

  _state.create_pipeline_cache();
}

// destructor
mr::Application::~Application()
{
  _state._device.destroy();
  _state._instance.destroy();
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

[[nodiscard]] std::unique_ptr<mr::Window>
mr::Application::create_window(size_t width, size_t height) const
{
  return std::make_unique<Window>(_state, width, height);
}

[[nodiscard]] mr::Mesh *mr::Application::create_mesh(
  std::span<PositionType> positions, std::span<FaceType> faces,
  std::span<ColorType> colors, std::span<TexCoordType> uvs,
  std::span<NormalType> normals, std::span<NormalType> tangents,
  std::span<NormalType> bitangent, std::span<BoneType> bones,
  BoundboxType bbox) const
{
  _tmp_mesh_pool.emplace_back(
    positions, faces, colors, uvs, normals, tangents, bitangent, bones, bbox);
  return &_tmp_mesh_pool.back();
}
