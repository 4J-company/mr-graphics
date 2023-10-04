#ifndef __image_hpp_
#define __image_hpp_

#include "pch.hpp"

#include "vulkan_application.hpp"

namespace mr
{
  class Image
  {
  private:
    vk::UniqueImage _image;
    vk::UniqueImageView _image_view;
    vk::UniqueDeviceMemory _memory;

    vk::Extent3D _extent;
    size_t _size;
    vk::Format _format;
    int _num_of_channels;
    uint _mip_level;

    vk::ImageLayout _layout;
    vk::ImageUsageFlags _usage_flags;
    vk::ImageAspectFlags _aspect_flags;

  public:
    Image() = default;

    Image(Image &&other) = default;
    Image &operator=(Image &&other) = default;

    Image(const VulkanState &state, uint width, uint height, vk::Format format, vk::Image image);
    Image(const VulkanState &state, uint width, uint height, vk::Format format,
          vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags, uint mip_level = 1);

    void switch_layout(const VulkanState &state, vk::ImageLayout new_layout);
    void copy_to_host() const;
    void get_pixel(const vk::Extent2D &coords) const;
    
    template<typename type>
    void write(const VulkanState &state, type *data)
    {
      Buffer stage_buffer = Buffer(state, _size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      stage_buffer.write(state, data);

      vk::ImageSubresourceLayers range
      {
        .aspectMask = _aspect_flags,
        .mipLevel = _mip_level - 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      };
      vk::BufferImageCopy region
      {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = range,
        .imageOffset = {0, 0, 0},
        .imageExtent = _extent,
      };

      // TODO: delete static
      static CommandUnit command_unit(state);
      command_unit.begin();
      command_unit->copyBufferToImage(stage_buffer.buffer(), _image.get(), _layout, {region});
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
    }

  private:
    void craete_image_view(const VulkanState &state);

  public:
    const vk::ImageView image_view() const { return _image_view.get(); }
    const vk::Image image() const { return _image.get(); }
    const vk::Format format() const { return _format; } 

  public:
    static vk::Format find_supported_format(const VulkanState &state, const std::vector<vk::Format> &candidates,
      vk::ImageTiling tiling, vk::FormatFeatureFlags features);
  };
} // namespace mr

#endif // __image_hpp_
