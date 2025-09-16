#include <vk_mem_alloc.h>

#include "resources/images/image.hpp"
#include "resources/buffer/buffer.hpp"
#include "vulkan_state.hpp"

// Utility function for image size calculation
static size_t calculate_image_size(mr::Extent extent, vk::Format format)
{
  return extent.width * extent.height * mr::format_byte_size(format);
}

// ---- Base Image ----

mr::Image::Image(const VulkanState &state, Extent extent, vk::Format format,
                 vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags,
                 vk::MemoryPropertyFlags memory_properties, uint mip_level)
  : _state(&state)
  , _mip_level(mip_level)
  , _extent{extent.width, extent.height, 1}
  , _size{calculate_image_size(extent, format)}
  , _format(format)
  , _layout(vk::ImageLayout::eUndefined)
  , _usage_flags(usage_flags)
  , _aspect_flags(aspect_flags)
  , _memory_properties(memory_properties)
{
  vk::ImageCreateInfo image_create_info {
    .imageType = vk::ImageType::e2D,
    .format = _format,
    .extent = _extent,
    .mipLevels = _mip_level,
    .arrayLayers = 1,
    .samples = vk::SampleCountFlagBits::e1,
    .tiling = vk::ImageTiling::eOptimal,
    .usage = _usage_flags,
    .sharingMode = vk::SharingMode::eExclusive,
    .initialLayout = _layout,
  };

  VmaAllocationCreateInfo allocation_create_info {
    .usage = VMA_MEMORY_USAGE_GPU_ONLY
  };

  auto result = vmaCreateImage(
    state.allocator(),
    (VkImageCreateInfo*)&image_create_info,
    &allocation_create_info,
    (VkImage*)&_image,
    &_allocation,
    nullptr
  );
  ASSERT(result == VK_SUCCESS, "Failed to create a vk::Image", result);

  create_image_view();
}

mr::Image::~Image() {
  if (_image != VK_NULL_HANDLE) {
    ASSERT(_state != nullptr);
    vmaDestroyImage(_state->allocator(), _image, _allocation);
    _image = VK_NULL_HANDLE;
  }
}

void mr::Image::switch_layout(vk::ImageLayout new_layout) {
  if (new_layout == _layout) {
    return;
  }

  vk::ImageSubresourceRange range {
    .aspectMask = _aspect_flags,
    .baseMipLevel = 0,
    .levelCount = _mip_level,
    .baseArrayLayer = 0,
    .layerCount = 1,
  };

  vk::ImageMemoryBarrier barrier {
    .oldLayout = _layout,
    .newLayout = new_layout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = _image,
    .subresourceRange = range
  };

  vk::PipelineStageFlags source_stage = vk::PipelineStageFlagBits::eAllCommands;
  vk::PipelineStageFlags destination_stage = vk::PipelineStageFlagBits::eAllCommands;

  switch (_layout) {
    case vk::ImageLayout::eUndefined:
      barrier.srcAccessMask = vk::AccessFlags();
      break;
    case vk::ImageLayout::ePreinitialized:
      barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
      break;
    case vk::ImageLayout::eColorAttachmentOptimal:
      barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
      break;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
      barrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
      break;
    case vk::ImageLayout::eTransferSrcOptimal:
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
      break;
    case vk::ImageLayout::eTransferDstOptimal:
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
      break;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
      barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
      break;
    case vk::ImageLayout::ePresentSrcKHR:
      barrier.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
      break;
    default:
      ASSERT(false, "Invalid source layout");
      break;
  }

  switch (new_layout) {
  case vk::ImageLayout::eTransferDstOptimal:
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    break;
  case vk::ImageLayout::eTransferSrcOptimal:
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    break;
  case vk::ImageLayout::eColorAttachmentOptimal:
    barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    break;
  case vk::ImageLayout::eDepthStencilAttachmentOptimal:
    barrier.dstAccessMask |= vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    break;
  case vk::ImageLayout::eShaderReadOnlyOptimal:
    if (barrier.srcAccessMask == vk::AccessFlags()) {
      barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite;
    }
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    break;
  case vk::ImageLayout::ePresentSrcKHR:
    barrier.dstAccessMask = vk::AccessFlagBits::eNoneKHR;
    break;
  default:
    ASSERT(false, "Invalid destination layout");
    break;
  }

  static CommandUnit command_unit(*_state);
  command_unit.begin();
  command_unit->pipelineBarrier(
    source_stage, destination_stage, {}, {}, {}, {barrier});
  command_unit.end();
  vk::SubmitInfo submit_info = command_unit.submit_info();

  auto fence = _state->device().createFenceUnique({}).value;
  _state->queue().submit(submit_info, fence.get());
  _state->device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

  _layout = new_layout;
}

void mr::Image::write(std::span<const std::byte> src) {
  ASSERT(_state != nullptr);
  ASSERT(src.data());
  ASSERT(src.size() <= _size);

  auto stage_buffer = HostBuffer(*_state, _size, vk::BufferUsageFlagBits::eTransferSrc);
  stage_buffer.write(std::span {src});

  vk::ImageSubresourceLayers range {
    .aspectMask = _aspect_flags,
      .mipLevel = _mip_level - 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };
  vk::BufferImageCopy region {
    .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = range,
      .imageOffset = {0, 0, 0},
      .imageExtent = _extent,
  };

  // TODO: delete static
  static CommandUnit command_unit(*_state);
  command_unit.begin();
  command_unit->copyBufferToImage(stage_buffer.buffer(), _image, _layout, {region});
  command_unit.end();

  vk::SubmitInfo submit_info = command_unit.submit_info();

  auto fence = _state->device().createFenceUnique({}).value;
  _state->queue().submit(submit_info, fence.get());
  _state->device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);
}

void mr::Image::create_image_view() {
  vk::ImageSubresourceRange range {
    .aspectMask = _aspect_flags,
    .baseMipLevel = 0,
    .levelCount = _mip_level,
    .baseArrayLayer = 0,
    .layerCount = 1,
  };

  vk::ImageViewCreateInfo create_info {
    .image = _image,
    .viewType = vk::ImageViewType::e2D,
    .format = _format,
    .components = {vk::ComponentSwizzle::eIdentity,
                   vk::ComponentSwizzle::eIdentity,
                   vk::ComponentSwizzle::eIdentity,
                   vk::ComponentSwizzle::eIdentity},
    .subresourceRange = range
  };

  _image_view = _state->device().createImageViewUnique(create_info).value;
}

vk::Format mr::Image::find_supported_format(
  const VulkanState &state, const std::vector<vk::Format> &candidates,
  vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
  for (auto format : candidates) {
    vk::FormatProperties props =
      state.phys_device().getFormatProperties(format);

    if (tiling == vk::ImageTiling::eLinear &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    }
    else if (tiling == vk::ImageTiling::eOptimal &&
             (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  ASSERT(false, "Can't find image format", candidates);
  return {};
}

mr::HostBuffer mr::Image::read_to_host_buffer(CommandUnit &command_unit) noexcept
{
  switch_layout(vk::ImageLayout::eTransferSrcOptimal);

  auto stage_buffer = HostBuffer(*_state, _size, vk::BufferUsageFlagBits::eTransferDst);
  vk::ImageSubresourceLayers range {
    .aspectMask = _aspect_flags,
    .mipLevel = _mip_level - 1,
    .baseArrayLayer = 0,
    .layerCount = 1,
  };
  vk::BufferImageCopy region {
    .bufferOffset = 0,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource = range,
    .imageOffset = {0, 0, 0},
    .imageExtent = _extent,
  };

  command_unit.begin();
  command_unit->copyImageToBuffer(_image, _layout, stage_buffer.buffer(), {region});
  command_unit.end();
  auto submit_info = command_unit.submit_info();

  auto fence = _state->device().createFenceUnique({}).value;
  _state->queue().submit(submit_info, fence.get());
  _state->device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

  return stage_buffer;
}

// ---- HostImage ----
mr::HostImage::HostImage(const VulkanState &state, Extent extent, vk::Format format,
                        vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags,
                        uint mip_level)
  : Image(state, extent, format, usage_flags, aspect_flags,
          vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, mip_level)
{}

// ---- DeviceImage ----
mr::DeviceImage::DeviceImage(const VulkanState &state, Extent extent, vk::Format format,
                            vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags,
                            uint mip_level)
  : Image(state, extent, format, usage_flags, aspect_flags,
          vk::MemoryPropertyFlagBits::eDeviceLocal, mip_level)
{}

// ---- SwapchainImage ----
mr::SwapchainImage::SwapchainImage(const VulkanState &state, Extent extent, vk::Format format, vk::Image image)
{
  _state = &state;
  _image = image;
  _extent = vk::Extent3D{.width = extent.width, .height= extent.height, .depth = 1};
  _size = calculate_image_size(extent, format);
  _format = format;
  _mip_level = 1;
  _aspect_flags = vk::ImageAspectFlagBits::eColor;
  _layout = vk::ImageLayout::eUndefined;
  create_image_view();
}

mr::SwapchainImage::SwapchainImage(
  const VulkanState &state,
  Extent extent,
  vk::Format format,
  vk::Image image,
  vk::ImageView view)
{
  _state = &state;
  _image = image; // Wrap, do not own
  _extent = vk::Extent3D{.width = extent.width, .height= extent.height, .depth = 1};
  _size = calculate_image_size(extent, format);
  _format = format;
  _mip_level = 1;
  _aspect_flags = vk::ImageAspectFlagBits::eColor;
  _layout = vk::ImageLayout::eUndefined;
  _image_view.reset(view);
}

mr::SwapchainImage::~SwapchainImage() {
  // We do not need to destroy swapchain image so we nullify it so that ~Image doesn't destroy it
  _image = VK_NULL_HANDLE;
}

// ---- TextureImage ----
mr::TextureImage::TextureImage(const VulkanState &state, Extent extent, vk::Format format,
                               vk::ImageUsageFlags usage_flags, uint mip_level)
    // TODO: replace transfer bit with StagingImage(?)
  : DeviceImage(state, extent, format,
                usage_flags | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                vk::ImageAspectFlagBits::eColor, mip_level)
{}

// ---- DepthImage ----
mr::DepthImage::DepthImage(const VulkanState &state, Extent extent, uint mip_level)
  : DeviceImage(
      state,
      extent,
      get_depthbuffer_format(state),
      vk::ImageUsageFlagBits::eDepthStencilAttachment,
      vk::ImageAspectFlagBits::eDepth,
      mip_level
    )
{
  switch_layout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

vk::RenderingAttachmentInfoKHR mr::DepthImage::attachment_info() const
{
  return vk::RenderingAttachmentInfoKHR {
    .imageView = _image_view.get(),
    .imageLayout = _layout,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = {vk::ClearDepthStencilValue(1.f, 0)},
  };
}

// ---- ColorAttachmentImage ----
mr::ColorAttachmentImage::ColorAttachmentImage(const VulkanState &state, Extent extent, vk::Format format, uint mip_level)
  : DeviceImage(state, extent, format,
    vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eTransferSrc,
    vk::ImageAspectFlagBits::eColor, mip_level)
{}

vk::RenderingAttachmentInfoKHR mr::ColorAttachmentImage::attachment_info() const
{
  return vk::RenderingAttachmentInfoKHR {
    .imageView = _image_view.get(),
    .imageLayout = _layout,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = {vk::ClearColorValue( std::array {0.f, 0.f, 0.f, 0.f})},
  };
}

// ---- StorageImage ----
mr::StorageImage::StorageImage(const VulkanState &state, Extent extent, vk::Format format, uint mip_level)
  : DeviceImage(state, extent, format, vk::ImageUsageFlagBits::eStorage, vk::ImageAspectFlagBits::eColor, mip_level)
{}
