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
                 vk::MemoryPropertyFlags memory_properties, uint mip_level, bool is_create_image_view)
  : _state(&state)
  , _mip_levels_number(mip_level)
  , _extent{extent.width, extent.height, 1}
  , _size{calculate_image_size(extent, format)}
  , _format(format)
  , _layout(vk::ImageLayout::eUndefined)
  , _aspect_flags(aspect_flags)
{
  ASSERT(mip_level > 0, "mip level number must be 1 or grater");

  vk::ImageCreateInfo image_create_info {
    .imageType = vk::ImageType::e2D,
    .format = _format,
    .extent = _extent,
    .mipLevels = _mip_levels_number,
    .arrayLayers = 1,
    .samples = vk::SampleCountFlagBits::e1,
    .tiling = vk::ImageTiling::eOptimal,
    .usage = usage_flags,
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

  if (result != VK_SUCCESS) {
#ifndef NDEBUG
    auto budgets = state.memory_budgets();
    for (uint32_t heapIndex = 0; heapIndex < VK_MAX_MEMORY_HEAPS; heapIndex++) {
      if (budgets[heapIndex].statistics.allocationCount == 0) continue;
      MR_DEBUG("My heap currently has {} allocations taking {} B,",
          budgets[heapIndex].statistics.allocationCount,
          budgets[heapIndex].statistics.allocationBytes);
      MR_DEBUG("allocated out of {} Vulkan device memory blocks taking {} B,",
          budgets[heapIndex].statistics.blockCount,
          budgets[heapIndex].statistics.blockBytes);
      MR_DEBUG("Vulkan reports total usage {} B with budget {} B ({}%).",
          budgets[heapIndex].usage,
          budgets[heapIndex].budget,
          budgets[heapIndex].usage / (float)budgets[heapIndex].budget);
    }
#endif
    ASSERT(result == VK_SUCCESS, "Failed to create a vk::Image", result, extent.width, extent.height, (int)format);
  }

  if (is_create_image_view) {
    _image_view = create_image_view(0, _mip_levels_number);
  }
}

mr::Image::Image(const VulkanState &state, const mr::importer::ImageData &image,
          vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags,
          vk::MemoryPropertyFlags memory_properties)
  : Image(state, image.extent(), image.format, usage_flags, aspect_flags, memory_properties, 1)
{
}

mr::Image::~Image() {
  if (_image != VK_NULL_HANDLE) {
    ASSERT(_state != nullptr);
    vmaDestroyImage(_state->allocator(), _image, _allocation);
    _image = VK_NULL_HANDLE;
  }
  ASSERT(_state != nullptr);
  _state->device().destroyImageView(_image_view);
}

void mr::Image::switch_layout(CommandUnit &command_unit, vk::ImageLayout new_layout)
{
  switch_layout(command_unit, new_layout, 0, _mip_levels_number);
}

void mr::Image::switch_layout(CommandUnit &command_unit, vk::ImageLayout new_layout,
                              uint32_t mip_level, uint32_t mip_counts)
{
  if (new_layout == _layout) {
    return;
  }

  vk::ImageSubresourceRange range {
    .aspectMask = _aspect_flags,
    .baseMipLevel = mip_level,
    .levelCount = mip_counts,
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

  command_unit->pipelineBarrier(source_stage, destination_stage, {}, {}, {}, {barrier});

  _layout = new_layout;
}

void mr::Image::write(CommandUnit &command_unit, std::span<const std::byte> src) {
  ASSERT(_state != nullptr);
  ASSERT(src.data());
  ASSERT(src.size() <= _size);

  command_unit.staging_buffers().emplace_back(*_state, _size, vk::BufferUsageFlagBits::eTransferSrc);
  command_unit.staging_buffers().back().write(std::span {src});

  vk::ImageSubresourceLayers range {
    .aspectMask = _aspect_flags,
      .mipLevel = _mip_levels_number - 1,
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

  command_unit->copyBufferToImage(command_unit.staging_buffers().back().buffer(), _image, _layout, {region});
}

vk::ImageView mr::Image::create_image_view(uint32_t mip_level, uint32_t mip_levels_count) {
  vk::ImageSubresourceRange range {
    .aspectMask = _aspect_flags,
    .baseMipLevel = mip_level,
    .levelCount = mip_levels_count,
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

  return _state->device().createImageView(create_info).value;
}

vk::Format mr::Image::find_supported_format(
  const VulkanState &state, const SmallVector<vk::Format> &candidates,
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
  vk::ImageSubresourceLayers range {
    .aspectMask = _aspect_flags,
    .mipLevel = _mip_levels_number - 1,
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

  auto stage_buffer = HostBuffer(*_state, _size, vk::BufferUsageFlagBits::eTransferDst);

  switch_layout(command_unit, vk::ImageLayout::eTransferSrcOptimal);
  command_unit->copyImageToBuffer(_image, _layout, stage_buffer.buffer(), {region});

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
                            uint mip_level, bool create_image_view)
  : Image(state, extent, format, usage_flags, aspect_flags,
          vk::MemoryPropertyFlagBits::eDeviceLocal, mip_level, create_image_view)
{}

mr::DeviceImage::DeviceImage(const VulkanState &state, const mr::importer::ImageData &image, vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags)
  : Image(state, image, usage_flags, aspect_flags, vk::MemoryPropertyFlagBits::eDeviceLocal)
{
}

// ---- SwapchainImage ----
mr::SwapchainImage::SwapchainImage(const VulkanState &state, Extent extent, vk::Format format, vk::Image image)
{
  _state = &state;
  _image = image;
  _extent = vk::Extent3D{.width = extent.width, .height= extent.height, .depth = 1};
  _size = calculate_image_size(extent, format);
  _format = format;
  _mip_levels_number = 1;
  _aspect_flags = vk::ImageAspectFlagBits::eColor;
  _layout = vk::ImageLayout::eUndefined;
  _image_view = create_image_view(0, _mip_levels_number);
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
  _mip_levels_number = 1;
  _aspect_flags = vk::ImageAspectFlagBits::eColor;
  _layout = vk::ImageLayout::eUndefined;
  _image_view = view;
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

mr::TextureImage::TextureImage(const VulkanState &state, const mr::importer::ImageData &image, vk::ImageUsageFlags usage_flags)
  : DeviceImage(state, image, usage_flags | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::ImageAspectFlagBits::eColor)
{
}

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
  CommandUnit command_unit {state};

  command_unit.begin();
  switch_layout(command_unit, vk::ImageLayout::eDepthStencilAttachmentOptimal);
  command_unit.end();

  UniqueFenceGuard(state.device(), command_unit.submit(state));
}

vk::RenderingAttachmentInfoKHR mr::DepthImage::attachment_info() const
{
  return vk::RenderingAttachmentInfoKHR {
    .imageView = _image_view,
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
    .imageView = _image_view,
    .imageLayout = _layout,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = {vk::ClearColorValue( std::array {0.f, 0.f, 0.f, 0.f})},
  };
}

// ---- StorageImage ----
mr::StorageImage::StorageImage(const VulkanState &state, Extent extent, vk::Format format, uint mip_level)
  : DeviceImage(state, extent, format, vk::ImageUsageFlagBits::eStorage, vk::ImageAspectFlagBits::eColor, mip_level)
{
  // switch_layout()
}

// ---- Depth pyramid image ----
mr::PyramidImage::PyramidImage(const VulkanState &state, Extent extent, vk::Format format, uint32_t mip_levels_number)
  : DeviceImage(state, extent, format, vk::ImageUsageFlagBits::eStorage, vk::ImageAspectFlagBits::eColor,
                mip_levels_number, false)
{
  _image_views.reserve(_mip_levels_number);
  for (uint32_t level = 0; level < _mip_levels_number; level++) {
    _image_views.emplace_back(create_image_view(level, 1));
  }
}

vk::ImageView mr::PyramidImage::get_level(uint32_t level) const noexcept
{
  ASSERT(level <= _mip_levels_number, "invalid mip level");
  return _image_views[level];
}
