#include "resources/images/image.hpp"
#include "resources/buffer/buffer.hpp"

/**
 * @brief Selects the first image format that supports the specified tiling and features.
 *
 * Iterates over a list of candidate image formats by querying the physical device properties,
 * and returns the first format that supports the required feature flags for the given tiling mode.
 * If none of the candidates satisfy the criteria, an assertion failure is triggered and an empty format is returned.
 *
 * @param state The Vulkan state used to access physical device properties.
 * @param candidates List of candidate image formats to evaluate.
 * @param tiling The desired image tiling mode (e.g., linear or optimal).
 * @param features The set of format feature flags that must be supported.
 * @return vk::Format A supported image format that meets the specified criteria.
 */
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

/**
 * @brief Computes the total size in bytes of an image based on its dimensions and format.
 *
 * This function determines the texel size from the provided Vulkan image format and calculates
 * the overall image size by multiplying the texel size with the total number of pixels (width * height)
 * in the given extent. The current implementation handles a few specific cases:
 * - For `vk::Format::eR8G8B8A8Srgb`, a texel size of 4 bytes is used.
 * - For `vk::Format::eR8G8B8Srgb`, a texel size of 3 bytes is used.
 * - For `vk::Format::eR8G8B8Srgb`, a texel size of 2 bytes is used.
 * - For any other format, a default texel size of 1 byte is assumed.
 *
 * @note The function does not yet support float formats.
 *
 * @param extent The dimensions of the image (width and height).
 * @param format The Vulkan image format that influences the texel size.
 * @return The total image size in bytes.
 */
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

/**
 * @brief Constructs an Image from an existing Vulkan image handle.
 *
 * This constructor initializes an Image instance using a pre-created Vulkan image. It sets the
 * image dimensions based on the provided extent (with a fixed depth of 1), calculates the total
 * image size using the calculate_image_size function, and assigns the image format and initial
 * layout. The default mip level is set to 1 and the aspect flags are configured for color.
 * An image view is created as part of the initialization, using the supplied Vulkan state.
 *
 * @param extent The dimensions (width and height) of the image.
 * @param format The format of the Vulkan image.
 * @param image The pre-existing Vulkan image handle.
 * @param swapchain_image Indicates whether the image is part of a swapchain.
 */
mr::Image::Image(const VulkanState &state, Extent extent,
                 vk::Format format, vk::Image image, bool swapchain_image)
  : _image{image}
  , _extent{extent.width, extent.height, 1}
  , _size{calculate_image_size(extent, format)}
  , _format{format}
  , _layout{vk::ImageLayout::eUndefined}
  , _holds_swapchain_image{swapchain_image}
{
  _mip_level = 1;
  _aspect_flags = vk::ImageAspectFlagBits::eColor;
  craete_image_view(state);
}

/**
 * @brief Constructs an Image by creating a Vulkan image with allocated device memory and an associated image view.
 *
 * This constructor initializes an Image using the provided extent, format, usage flags, aspect flags, and mip levels.
 * It calculates the image size based on the extent and format, creates a Vulkan image with optimal tiling and exclusive sharing,
 * allocates device-local memory for the image, binds the memory, and sets up an image view.
 *
 * @param extent The dimensions of the image (width and height; depth is set to 1).
 * @param format The pixel format of the image.
 * @param usage_flags Flags specifying the permitted usage for the image (e.g., sampled, color attachment).
 * @param aspect_flags Flags indicating which aspect(s) of the image are accessible (e.g., color, depth).
 * @param mip_level The number of mipmap levels for the image.
 */
mr::Image::Image(const VulkanState &state, Extent extent,
                 vk::Format format, vk::ImageUsageFlags usage_flags,
                 vk::ImageAspectFlags aspect_flags, uint mip_level)
  : _mip_level{mip_level}
  , _extent{extent.width, extent.height, 1}
  , _size{calculate_image_size(extent, format)}
  , _format{format}
  , _layout{vk::ImageLayout::eUndefined}
  , _usage_flags{usage_flags}
  , _aspect_flags{aspect_flags}
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
                               vk::MemoryPropertyFlagBits::eDeviceLocal),
  };

  _memory = state.device().allocateMemoryUnique(alloc_info, nullptr).value;
  state.device().bindImageMemory(_image.get(), _memory.get(), 0);

  craete_image_view(state);
}

mr::Image::~Image() {
  // swapchain images are destroyed with owning swapchain
  if (_holds_swapchain_image)
    _image.release();
}

void mr::Image::craete_image_view(const VulkanState &state)
{
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

void mr::Image::switch_layout(const VulkanState &state,
                              vk::ImageLayout new_layout)
{
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
    .subresourceRange = range};

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
    assert(false); // cant chose layout
  }

  // TODO: delete static
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
  auto fence = state.device().createFence({}).value;
  state.queue().submit(submit_info, fence);
  state.device().waitForFences({fence}, VK_TRUE, UINT64_MAX);

  _layout = new_layout;
}

void mr::Image::copy_to_host() const {}

void mr::Image::get_pixel(const vk::Extent2D &coords) const {}
