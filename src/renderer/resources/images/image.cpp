#include "resources/images/image.hpp"
#include "resources/buffer/buffer.hpp"

// Utility function for image size calculation
static size_t calculate_image_size(mr::Extent extent, vk::Format format)
{
  // TODO: support float formats
  size_t texel_size =
    format == vk::Format::eR8G8B8A8Srgb ? 4
    : format == vk::Format::eR8G8B8Srgb ? 3
    : format == vk::Format::eR8G8B8Srgb ? 2
                                        : 1;
  return extent.width * extent.height * texel_size;
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
  _image = state.device().createImageUnique(image_create_info).value;

  vk::MemoryRequirements mem_requirements =
    state.device().getImageMemoryRequirements(_image.get());
  vk::MemoryAllocateInfo alloc_info {
    .allocationSize = mem_requirements.size,
    .memoryTypeIndex =
      Buffer::find_memory_type(state,
                               mem_requirements.memoryTypeBits,
                               _memory_properties),
  };

  _memory = state.device().allocateMemoryUnique(alloc_info, nullptr).value;
  state.device().bindImageMemory(_image.get(), _memory.get(), 0);

  create_image_view(state);
}

mr::Image::~Image() {}

void mr::Image::switch_layout(const VulkanState &state, vk::ImageLayout new_layout) {
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
    .image = _image.get(),
    .subresourceRange = range
  };

  vk::PipelineStageFlags source_stage;
  vk::PipelineStageFlags destination_stage;

  if (_layout == vk::ImageLayout::eUndefined &&
      new_layout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    destination_stage = vk::PipelineStageFlagBits::eTransfer;
  }
  else if (_layout == vk::ImageLayout::eTransferDstOptimal &&
           new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    source_stage = vk::PipelineStageFlagBits::eTransfer;
    destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
  }
  else if (_layout == vk::ImageLayout::eUndefined &&
           new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead |
                            vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    destination_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
  }
  else {
    assert(false); // can't choose layout
  }

  static CommandUnit command_unit(state);
  command_unit.begin();
  command_unit->pipelineBarrier(
    source_stage, destination_stage, {}, {}, {}, {barrier});
  command_unit.end();

  auto [bufs, size] = command_unit.submit_info();
  vk::SubmitInfo submit_info {
    .commandBufferCount = size,
    .pCommandBuffers = bufs,
  };
  auto fence = state.device().createFenceUnique({}).value;
  state.queue().submit(submit_info, fence.get());
  state.device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

  _layout = new_layout;
}

void mr::Image::copy_to_host() const {}
void mr::Image::get_pixel(const vk::Extent2D &coords) const {}

void mr::Image::create_image_view(const VulkanState &state) {
  vk::ImageSubresourceRange range {
    .aspectMask = _aspect_flags,
    .baseMipLevel = 0,
    .levelCount = _mip_level,
    .baseArrayLayer = 0,
    .layerCount = 1,
  };

  vk::ImageViewCreateInfo create_info {
    .image = _image.get(),
    .viewType = vk::ImageViewType::e2D,
    .format = _format,
    .components = {vk::ComponentSwizzle::eIdentity,
                   vk::ComponentSwizzle::eIdentity,
                   vk::ComponentSwizzle::eIdentity,
                   vk::ComponentSwizzle::eIdentity},
    .subresourceRange = range
  };

  _image_view = state.device().createImageViewUnique(create_info).value;
}

// Template write implementation
// (kept as a template in the header)

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
  assert(false); // cant find format
  return {};
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
  _image.reset(image); // Wrap, do not own
  _extent = {extent.width, extent.height, 1};
  _size = calculate_image_size(extent, format);
  _format = format;
  _mip_level = 1;
  _aspect_flags = vk::ImageAspectFlagBits::eColor;
  _layout = vk::ImageLayout::eUndefined;
  create_image_view(state);
}

mr::SwapchainImage::~SwapchainImage() {
  // Do not destroy swapchain image, just release wrapper
  _image.release();
}

// ---- TextureImage ----
mr::TextureImage::TextureImage(const VulkanState &state, Extent extent, vk::Format format, vk::ImageUsageFlags usage_flags, uint mip_level)
  : DeviceImage(state, extent, format, usage_flags, vk::ImageAspectFlagBits::eColor, mip_level)
{}

// ---- DepthImage ----
mr::DepthImage::DepthImage(const VulkanState &state, Extent extent, vk::Format format, uint mip_level)
  : DeviceImage(state, extent, format, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageAspectFlagBits::eDepth, mip_level)
{}

// ---- ColorAttachmentImage ----
mr::ColorAttachmentImage::ColorAttachmentImage(const VulkanState &state, Extent extent, vk::Format format, uint mip_level)
  : DeviceImage(state, extent, format, vk::ImageUsageFlagBits::eColorAttachment, vk::ImageAspectFlagBits::eColor, mip_level)
{}

// ---- StorageImage ----
mr::StorageImage::StorageImage(const VulkanState &state, Extent extent, vk::Format format, uint mip_level)
  : DeviceImage(state, extent, format, vk::ImageUsageFlagBits::eStorage, vk::ImageAspectFlagBits::eColor, mip_level)
{}
