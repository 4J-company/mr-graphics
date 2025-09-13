#ifndef __MR_IMAGE_HPP_
#define __MR_IMAGE_HPP_

#include "pch.hpp"

#include "vulkan_state.hpp"

namespace mr {
inline namespace graphics {
  class Image {
    protected:
      vk::UniqueImage _image;
      vk::UniqueImageView _image_view;
      vk::UniqueDeviceMemory _memory;

      vk::Extent3D _extent;
      size_t _size{};
      vk::Format _format;
      int _num_of_channels{};
      uint _mip_level{};
      vk::ImageLayout _layout;
      vk::ImageUsageFlags _usage_flags;
      vk::ImageAspectFlags _aspect_flags;
      vk::MemoryPropertyFlags _memory_properties;
      const VulkanState *_state = nullptr;

      // Protected constructor for use by derived classes
      Image() = default;
      Image(const VulkanState &state, Extent extent, vk::Format format,
            vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags,
            vk::MemoryPropertyFlags memory_properties, uint mip_level = 1);

    public:
      Image(Image &&) = default;
      Image& operator=(Image &&) = default;

      virtual ~Image();

      void switch_layout(const VulkanState &state, vk::ImageLayout new_layout);
      virtual void copy_to_host() const;
      virtual void get_pixel(const vk::Extent2D &coords) const;

      void write(const VulkanState &state, std::span<const std::byte> src);

      template <typename T>
      void write(const VulkanState &state, std::span<T> src) { write(state, std::as_bytes(src)); }

    protected:
      void create_image_view(const VulkanState &state);

    public:
      vk::ImageView image_view() const noexcept { return _image_view.get(); }
      vk::Image image() const noexcept { return _image.get(); }
      vk::Format format() const noexcept { return _format; }
      size_t size() const noexcept { return _size; }
      vk::MemoryPropertyFlags memory_properties() const noexcept { return _memory_properties; }

      static vk::Format find_supported_format(
        const VulkanState &state, const std::vector<vk::Format> &candidates,
        vk::ImageTiling tiling, vk::FormatFeatureFlags features);
  };

  // HostImage: host-visible, for staging or CPU read/write
  class HostImage : public Image {
    public:
      HostImage(const VulkanState &state, Extent extent, vk::Format format,
                vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags,
                uint mip_level = 1);
      HostImage(HostImage&&) noexcept = default;
      HostImage & operator=(HostImage&&) noexcept = default;
      ~HostImage() override = default;
  };

  // DeviceImage: device-local, for optimal GPU access
  class DeviceImage : public Image {
    public:
      DeviceImage(const VulkanState &state, Extent extent, vk::Format format,
                  vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags,
                  uint mip_level = 1);
      DeviceImage(DeviceImage&&) noexcept = default;
      DeviceImage & operator=(DeviceImage&&) noexcept = default;
      ~DeviceImage() override = default;
  };

  class SwapchainImage : public Image {
    public:
      SwapchainImage(const VulkanState &state, Extent extent, vk::Format format, vk::Image image);
      SwapchainImage(const VulkanState &state, Extent extent, vk::Format format, vk::Image image, vk::ImageView view);
      SwapchainImage(SwapchainImage&&) noexcept = default;
      SwapchainImage & operator=(SwapchainImage&&) noexcept = default;
      ~SwapchainImage() override;

  };

  // TextureImage: for sampled images (textures)
  class TextureImage : public DeviceImage {
    public:
      TextureImage(const VulkanState &state, Extent extent, vk::Format format, vk::ImageUsageFlags usage_flags = {}, uint mip_level = 1);
      TextureImage(TextureImage&&) noexcept = default;
      TextureImage & operator=(TextureImage&&) noexcept = default;
      ~TextureImage() override = default;
      // Add mipmap generation, upload helpers as needed
  };

  // DepthImage: for depth/stencil attachments
  class DepthImage : public DeviceImage {
    public:
      DepthImage(const VulkanState &state, Extent extent, uint mip_level = 1);
      DepthImage(DepthImage&&) noexcept = default;
      DepthImage & operator=(DepthImage&&) noexcept = default;
      ~DepthImage() override = default;

      vk::RenderingAttachmentInfoKHR attachment_info() const;
  };

  // Optional: ColorAttachmentImage, StorageImage (stubs)
  class ColorAttachmentImage : public DeviceImage {
    public:
      ColorAttachmentImage(const VulkanState &state, Extent extent, vk::Format format, uint mip_level = 1);
      ColorAttachmentImage(ColorAttachmentImage&&) noexcept = default;
      ColorAttachmentImage & operator=(ColorAttachmentImage&&) noexcept = default;
      ~ColorAttachmentImage() override = default;

      vk::RenderingAttachmentInfoKHR attachment_info() const;
  };

  class StorageImage : public DeviceImage {
    public:
      StorageImage(const VulkanState &state, Extent extent, vk::Format format, uint mip_level = 1);
      StorageImage(StorageImage&&) noexcept = default;
      StorageImage & operator=(StorageImage&&) noexcept = default;
      ~StorageImage() override = default;
  };

  inline vk::Format get_swapchain_format(const VulkanState& state) {
    return vk::Format::eB8G8R8A8Unorm;
  }

  inline vk::Format get_depthbuffer_format(const VulkanState& state) {
    static vk::Format format =
      Image::find_supported_format(
        state,
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
      );
    return format;
  }
}
} // namespace mr

#endif // __MR_IMAGE_HPP_
