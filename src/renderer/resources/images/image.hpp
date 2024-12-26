#ifndef __MR_IMAGE_HPP_
#define __MR_IMAGE_HPP_

#include "pch.hpp"

#include "vulkan_state.hpp"

namespace mr {
  class Image {
    private:
      vk::UniqueImage _image;
      vk::UniqueImageView _image_view;
      vk::UniqueDeviceMemory _memory;

      vk::Extent3D _extent;
      size_t _size{};
      vk::Format _format;
      int _num_of_channels{};
      uint _mip_level{};
      bool _holds_swapchain_image{};

      vk::ImageLayout _layout;
      vk::ImageUsageFlags _usage_flags;
      vk::ImageAspectFlags _aspect_flags;

    public:
      Image() = default;

      Image(const VulkanState &state, Extent extent,
            vk::Format format, vk::Image image, bool swapchain_image);

      Image(const VulkanState &state, Extent extent,
            vk::Format format, vk::ImageUsageFlags usage_flags,
            vk::ImageAspectFlags aspect_flags, uint mip_level = 1);

      Image(Image &&) = default;
      Image & operator =(Image &&) = default;
      ~Image();

      void switch_layout(const VulkanState &state, vk::ImageLayout new_layout);
      void copy_to_host() const;
      void get_pixel(const vk::Extent2D &coords) const;

      template <typename T>
      void write(const VulkanState &state, std::span<const T> src)
      {
        size_t byte_size = src.size() * sizeof(T);

        assert(byte_size <= _size);

        auto stage_buffer =
          HostBuffer(state, _size, vk::BufferUsageFlagBits::eTransferSrc);
        stage_buffer.write(state, std::span {src});

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
        static CommandUnit command_unit(state);
        command_unit.begin();
        command_unit->copyBufferToImage(
          stage_buffer.buffer(), _image.get(), _layout, {region});
        command_unit.end();

        auto [bufs, bufs_number] = command_unit.submit_info();
        vk::SubmitInfo submit_info {
          .commandBufferCount = bufs_number,
          .pCommandBuffers = bufs,
        };
        auto fence = state.device().createFence({}).value;
        state.queue().submit(submit_info, fence);
        state.device().waitForFences({fence}, VK_TRUE, UINT64_MAX);
      }

    private:
      void craete_image_view(const VulkanState &state);

    public:
      vk::ImageView image_view() const noexcept { return _image_view.get(); }
      vk::Image image() const noexcept { return _image.get(); }
      vk::Format format() const noexcept { return _format; }
      size_t size() const noexcept { return _size; }

    public:
      static vk::Format find_supported_format(
        const VulkanState &state, const std::vector<vk::Format> &candidates,
        vk::ImageTiling tiling, vk::FormatFeatureFlags features);
  };
} // namespace mr

#endif // __MR_IMAGE_HPP_
