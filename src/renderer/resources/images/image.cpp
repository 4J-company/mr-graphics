#include "resources/images/image.hpp"
#include "resources/buffer/buffer.hpp"

mr::Image::Image(const VulkanState &state, uint width, uint height, vk::Format format, vk::Image image)
    : _image(image), _extent({width, height}), _format(format), _layout(vk::ImageLayout::eUndefined)
{
  _mip_level = 1;
  _aspect_flags = vk::ImageAspectFlagBits::eColor;
  craete_image_view(state);
}

mr::Image::Image(const VulkanState &state, uint width, uint height, vk::Format format,
                 vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags, uint mip_level)
  : _mip_level(mip_level), _extent({width, height, 1}), _format(format), _size(width * height * 4), 
    _layout(vk::ImageLayout::eUndefined), _usage_flags(usage_flags), _aspect_flags(aspect_flags)
{
  vk::ImageCreateInfo image_create_info
  {
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

  vk::MemoryRequirements mem_requirements = state.device().getImageMemoryRequirements(_image.get());
  vk::MemoryAllocateInfo alloc_info 
  {
    .allocationSize = mem_requirements.size,
    .memoryTypeIndex = Buffer::find_memory_type(state, mem_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal),
  };

  _memory = state.device().allocateMemoryUnique(alloc_info, nullptr).value;
  state.device().bindImageMemory(_image.get(), _memory.get(), 0);

  craete_image_view(state);
}

void mr::Image::craete_image_view(const VulkanState &state)
{
  vk::ImageSubresourceRange range 
  {
    .aspectMask = _aspect_flags,
    .baseMipLevel = 0,
    .levelCount = _mip_level,
    .baseArrayLayer = 0,
    .layerCount = 1,
  };

  vk::ImageViewCreateInfo create_info {.image = _image.get(),
                                       .viewType = vk::ImageViewType::e2D,
                                       .format = _format,
                                       .components = {vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                                                      vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity},
                                       .subresourceRange = range};

  _image_view = state.device().createImageViewUnique(create_info).value;
}

void mr::Image::switch_layout(const VulkanState &state, vk::ImageLayout new_layout) 
{
  vk::ImageSubresourceRange range
  {
    .aspectMask = _aspect_flags,
    .baseMipLevel = 0,
    .levelCount = _mip_level,
    .baseArrayLayer = 0,
    .layerCount = 1,
  };

  vk::ImageMemoryBarrier barrier 
  {
    .oldLayout = _layout,
    .newLayout = new_layout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    
    .image = _image.get(),
    .subresourceRange = range
  };

  vk::PipelineStageFlags source_stage;
  vk::PipelineStageFlags destination_stage;

  if (_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal)
  {
    barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    destination_stage = vk::PipelineStageFlagBits::eTransfer;
  }
  else if (_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal)
  {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    source_stage = vk::PipelineStageFlagBits::eTransfer;
    destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
  }
  else
    assert(false); // cant chose layout

  // TODO: delete static
  static CommandUnit command_unit(state);
  command_unit.begin();
  command_unit->pipelineBarrier(source_stage, destination_stage, {}, {}, {}, {barrier});
  command_unit.end();

  auto [bufs, size] = command_unit.submit_info();
  vk::SubmitInfo submit_info 
  {
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
