#include "window_context.hpp"
#include "renderer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "resources/pipelines/graphics_pipeline.hpp"
#include <vulkan/vulkan_enums.hpp>

mr::WindowContext::WindowContext(Window *parent, const VulkanState &state) : _parent(parent), _state(state)
{
  _surface.reset(xgfx::createSurface(_parent->xwindow_ptr(), state.instance()));

  size_t universal_queue_family_id = 0;

  // Choose surface format
  const auto avaible_formats = state.phys_device().getSurfaceFormatsKHR(_surface.get()).value;
  _swapchain_format = avaible_formats[0].format;
  for (const auto format : avaible_formats)
    if (format.format == vk::Format::eB8G8R8A8Unorm) // Prefered format
    {
      _swapchain_format = format.format;
      break;
    }
    else if (format.format == vk::Format::eR8G8B8A8Unorm)
      _swapchain_format = format.format;

  // Choose surface extent
  vk::SurfaceCapabilitiesKHR surface_capabilities;
  surface_capabilities = state.phys_device().getSurfaceCapabilitiesKHR(_surface.get()).value;
  if (surface_capabilities.currentExtent.width == std::numeric_limits<uint>::max())
  {
    // If the surface size is undefined, the size is set to the size of the images requested.
    _extent.width = std::clamp(static_cast<uint32_t>(_parent->width()), surface_capabilities.minImageExtent.width,
                               surface_capabilities.maxImageExtent.width);
    _extent.height = std::clamp(static_cast<uint32_t>(_parent->height()), surface_capabilities.minImageExtent.height,
                                surface_capabilities.maxImageExtent.height);
  }
  else // If the surface size is defined, the swap chain size must match
    _extent = surface_capabilities.currentExtent;

  // Chosse surface present mode
  const auto avaible_present_modes = state.phys_device().getSurfacePresentModesKHR(_surface.get()).value;
  const auto iter = std::find(avaible_present_modes.begin(), avaible_present_modes.end(), vk::PresentModeKHR::eMailbox);
  auto present_mode = (iter != avaible_present_modes.end())
      ? *iter
      : vk::PresentModeKHR::eFifo; // FIFO is required to be supported (by spec)

  // Choose surface transform
  vk::SurfaceTransformFlagBitsKHR pre_transform =
      (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
      ? vk::SurfaceTransformFlagBitsKHR::eIdentity
      : surface_capabilities.currentTransform;

  // Choose composite alpha
  vk::CompositeAlphaFlagBitsKHR composite_alpha =
      (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
      ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
      : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
      ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
      : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit)
      ? vk::CompositeAlphaFlagBitsKHR::eInherit
      : vk::CompositeAlphaFlagBitsKHR::eOpaque;

  /// _swapchain_format = vk::Format::eB8G8R8A8Srgb; // ideal, we have eB8G8R8A8Unorm
  _swapchain_format = vk::Format::eB8G8R8A8Unorm;
  vk::SwapchainCreateInfoKHR swapchain_create_info {.flags = {},
                                                    .surface = _surface.get(),
                                                    .minImageCount = Framebuffer::max_presentable_images,
                                                    .imageFormat = _swapchain_format,
                                                    .imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
                                                    .imageExtent = _extent,
                                                    .imageArrayLayers = 1,
                                                    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
                                                    .imageSharingMode = vk::SharingMode::eExclusive,
                                                    .queueFamilyIndexCount = {},
                                                    .pQueueFamilyIndices = {},
                                                    .preTransform = pre_transform,
                                                    .compositeAlpha = composite_alpha,
                                                    .presentMode = present_mode,
                                                    .clipped = true,
                                                    .oldSwapchain = nullptr};

  uint32_t indeces[1] {static_cast<uint32_t>(universal_queue_family_id)};
  swapchain_create_info.queueFamilyIndexCount = 1;
  swapchain_create_info.pQueueFamilyIndices = indeces;

  _swapchain = state.device().createSwapchainKHRUnique(swapchain_create_info).value;

  create_framebuffers(state);

  _image_available_semaphore = _state.device().createSemaphore({}).value;
  _render_rinished_semaphore = _state.device().createSemaphore({}).value;
  _image_fence = _state.device().createFence({.flags = vk::FenceCreateFlagBits::eSignaled}).value;
}

void mr::WindowContext::create_framebuffers(const VulkanState &state)
{
  auto swampchain_images = state.device().getSwapchainImagesKHR(_swapchain.get()).value;

  for (uint i = 0; i < Framebuffer::max_presentable_images; i++)
    _framebuffers[i] = Framebuffer(state, _extent.width, _extent.height, _swapchain_format, swampchain_images[i]);
}

namespace mr
{
  // TODO: allocating vertex and index buffers in gpu memory
  template<typename type = float>
  mr::Buffer create_vertex_buffer(const VulkanState &state, size_t size, type *data = nullptr)
  {
    auto buf = Buffer(state, size, vk::BufferUsageFlagBits::eVertexBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    if (data != nullptr)
      buf.write(state, data);
    return buf;
  }

  template<typename type = float>
  mr::Buffer create_index_buffer(const VulkanState &state, size_t size, type *data = nullptr)
  {
    auto buf = Buffer(state, size, vk::BufferUsageFlagBits::eIndexBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    if (data != nullptr)
      buf.write(state, data);
    return buf;
  }

  template<typename type = float>
  mr::Buffer create_uniform_buffer(const VulkanState &state, size_t size, type *data = nullptr)
  {
    auto buf = Buffer(state, size, vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    if (data != nullptr)
      buf.write(state, data);
    return buf;
  }
}

void mr::WindowContext::render()
{
  static Shader _shader = Shader(_state, "default");
  std::vector<vk::VertexInputAttributeDescription> descrs
  {
    { .location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = 0 },
    { .location = 1, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = 2 * sizeof(float) }
  };
  
  std::vector<vk::DescriptorSetLayoutBinding> bindings
  { 
    {
      .binding = 0,
      .descriptorType = vk::DescriptorType::eCombinedImageSampler,
      .descriptorCount = 1,
      .stageFlags = vk::ShaderStageFlagBits::eFragment,
    },
    {
      .binding = 1,
      .descriptorType = vk::DescriptorType::eUniformBuffer,
      .descriptorCount = 1,
      .stageFlags = vk::ShaderStageFlagBits::eVertex,
    }
  };
  static GraphicsPipeline pipeline = GraphicsPipeline(_state, &_shader, descrs, {bindings});

  float matr[16]
  {
    -1.41421354, 0.816496611, -0.578506112, -0.577350259,
    0.00000000, -1.63299322, -0.578506112, -0.577350259,
    1.41421354, 0.816496611, -0.578506112, -0.577350259,
    0.00000000, 0.00000000, 34.5101662, 34.6410141,
  };

  static Buffer uniiform_buffer = create_uniform_buffer(_state, sizeof(float) * 16);
  uniiform_buffer.write(_state, matr);
  static Texture texture = Texture(_state, "bin/textures/cat.png");
  std::vector<DescriptorAttachment> attach { { .texture = &texture }, { .uniform_buffer = &uniiform_buffer } };
  static Descriptor set = Descriptor(_state, &pipeline, attach);

  static CommandUnit command_unit {_state};

  struct vec2 { float x, y; };
  struct vertex { vec2 coord, tex; };
  std::vector<vertex> vertexes
  {
    {{-0.5, -0.5}, {0, 0}},
    {{0.5, -0.5}, {1, 0}},
    {{0.5, 0.5}, {1, 1}},
    {{-0.5, 0.5}, {0, 1}},
  };
  std::vector<int> indexes {0, 1, 2, 2, 3, 0};
  static Buffer vertex_buffer = create_vertex_buffer(_state, sizeof(vertexes[0]) * vertexes.size(), vertexes.data());
  static Buffer index_buffer = create_index_buffer(_state, sizeof(indexes[0]) * indexes.size(), indexes.data());

  _state.device().waitForFences(_image_fence, VK_TRUE, UINT64_MAX);
  _state.device().resetFences(_image_fence);
  mr::uint image_index = 0;

  _state.device().acquireNextImageKHR(_swapchain.get(), UINT64_MAX, _image_available_semaphore, nullptr, &image_index);
  command_unit.begin();

  vk::ClearValue clear_color {vk::ClearColorValue(0, 0, 0, 0)};
  vk::RenderPassBeginInfo render_pass_info {
      .renderPass = _state.render_pass(),
      .framebuffer = _framebuffers[image_index].framebuffer(),
      .renderArea = {{0, 0}, _extent},
      .clearValueCount = 1,
      .pClearValues = &clear_color,
  };

  command_unit->beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
  command_unit->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.pipeline());
  command_unit->setViewport(0, _framebuffers[image_index].viewport());
  command_unit->setScissor(0, _framebuffers[image_index].scissors());
  command_unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.layout(), 0, {set.set()}, {});
  command_unit->bindVertexBuffers(0, {vertex_buffer.buffer()}, {0});
  command_unit->bindIndexBuffer(index_buffer.buffer(), 0, vk::IndexType::eUint32);
  command_unit->drawIndexed(indexes.size(), 1, 0, 0, 0);

  command_unit.end();

  vk::Semaphore wait_semaphores[] = {_image_available_semaphore};
  vk::PipelineStageFlags wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
  vk::Semaphore signal_semaphores[] = {_render_rinished_semaphore};

    auto [bufs, size] = command_unit.submit_info();
  vk::SubmitInfo submit_info {
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = wait_semaphores,
      .pWaitDstStageMask = wait_stages,
      .commandBufferCount = size,
      .pCommandBuffers = bufs,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = signal_semaphores,
  };

  _state.queue().submit(submit_info, _image_fence);

  vk::SwapchainKHR swapchains[] = {_swapchain.get()};
  vk::PresentInfoKHR present_info {
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = signal_semaphores,
      .swapchainCount = 1,
      .pSwapchains = swapchains,
      .pImageIndices = &image_index,
      .pResults = nullptr, // Optional
  };
  _state.queue().presentKHR(present_info);
}
