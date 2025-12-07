#ifndef __MR_IMAGE_HPP_
#define __MR_IMAGE_HPP_

#include "pch.hpp"

#include "vulkan_state.hpp"

namespace mr {
inline namespace graphics {
  class Image {
  protected:
    vk::Image _image;          // this is not Unique because it's handled by VMA
    vk::ImageView _image_view; // these are not Unique to be destroyed before _image

    VmaAllocation _allocation;

    vk::Extent3D _extent;
    size_t _size{};
    vk::Format _format;
    int _num_of_channels{};
    uint _mip_levels_number{};
    vk::ImageLayout _layout;
    vk::ImageAspectFlags _aspect_flags;
    const VulkanState *_state = nullptr;

    // Protected constructor for use by derived classes
    Image() = default;
    Image(const VulkanState &state, Extent extent, vk::Format format,
          vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags,
          vk::MemoryPropertyFlags memory_properties, uint mip_level = 1,
          bool create_image_view = true);
    Image(const VulkanState &state, const mr::importer::ImageData &image,
          vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags,
          vk::MemoryPropertyFlags memory_properties);

  public:
    Image(Image &&other) noexcept {
      std::swap(_state, other._state);

      std::swap(_image, other._image);
      std::swap(_image_view, other._image_view);
      std::swap(_allocation, other._allocation);

      std::swap(_extent, other._extent);
      std::swap(_size, other._size);
      std::swap(_format, other._format);
      std::swap(_num_of_channels, other._num_of_channels);
      std::swap(_mip_levels_number, other._mip_levels_number);
      std::swap(_layout, other._layout);
      std::swap(_aspect_flags, other._aspect_flags);
    }
    Image& operator=(Image &&other) noexcept {
      std::swap(_state, other._state);

      std::swap(_image, other._image);
      std::swap(_image_view, other._image_view);
      std::swap(_allocation, other._allocation);

      std::swap(_extent, other._extent);
      std::swap(_size, other._size);
      std::swap(_format, other._format);
      std::swap(_num_of_channels, other._num_of_channels);
      std::swap(_mip_levels_number, other._mip_levels_number);
      std::swap(_layout, other._layout);
      std::swap(_aspect_flags, other._aspect_flags);

      return *this;
    }

    virtual ~Image();

    void switch_layout(CommandUnit &command_unit, vk::ImageLayout new_layout);
    void switch_layout(CommandUnit &command_unit, vk::ImageLayout new_layout,
                       uint32_t mip_level, uint32_t mip_counts, bool ignore_prev_layout = false);

    // Copy data from to host visible buffer
    HostBuffer read_to_host_buffer(CommandUnit &command_unit) noexcept;

    // TODO(dk6): implement this
    // std::vector<std::byte> read() { return read_to_buffer().read(); }
    // Vec4f get_pixel(x, y) -> get small size (16 bytes)

  public:
    void write(CommandUnit &command_unit, std::span<const std::byte> src);

    template <typename T>
    void write(CommandUnit &command_unit, std::span<T> src) { write(command_unit, std::as_bytes(src)); }

  protected:
    vk::ImageView create_image_view(uint32_t mip_level, uint32_t mip_levels_count);

  public:
    vk::ImageView image_view() const noexcept { return _image_view; }
    vk::Image image() const noexcept { return _image; }
    vk::Format format() const noexcept { return _format; }

    const vk::Extent3D & extent() const noexcept { return _extent; }
    size_t size() const noexcept { return _size; }
    uint32_t mip_levels_number() const noexcept { return _mip_levels_number; }

    static vk::Format find_supported_format(
      const VulkanState &state, const SmallVector<vk::Format> &candidates,
      vk::ImageTiling tiling, vk::FormatFeatureFlags features);

    static bool is_image_format_supported(
        const VulkanState &state,
        vk::Format format,
        vk::ImageType imageType,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage)
    {
      vk::PhysicalDeviceImageFormatInfo2 formatInfo {
        .format = format,
        .type = imageType,
        .tiling = tiling,
        .usage = usage,
        .flags = vk::ImageCreateFlagBits(0),
      };

      vk::ImageFormatProperties2 formatProperties { };

      vk::Result result = (vk::Result)vkGetPhysicalDeviceImageFormatProperties2(
        state.phys_device(),
        (VkPhysicalDeviceImageFormatInfo2*)&formatInfo,
        (VkImageFormatProperties2*)&formatProperties
      );

      return result == vk::Result::eSuccess;
    }
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
                  uint mip_level = 1, bool create_image_view = true);
      DeviceImage(const VulkanState &state, const mr::importer::ImageData &image, vk::ImageUsageFlags usage_flags, vk::ImageAspectFlags aspect_flags);
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
      TextureImage(const VulkanState &state, const mr::importer::ImageData &image, vk::ImageUsageFlags usage_flags = {});
      TextureImage(TextureImage&&) noexcept = default;
      TextureImage & operator=(TextureImage&&) noexcept = default;
      ~TextureImage() override = default;
      // Add mipmap generation, upload helpers as needed

      static bool is_texture_format_supported(const VulkanState &state, vk::Format format) {
        return is_image_format_supported(
          state,
          format,
          vk::ImageType::e2D,
          vk::ImageTiling::eOptimal,
          vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled
        );
      }
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
      StorageImage(const VulkanState &state, Extent extent, vk::Format format,
                   uint mip_level = 1, bool create_image_view = false);
      StorageImage(StorageImage&&) noexcept = default;
      StorageImage & operator=(StorageImage&&) noexcept = default;
      ~StorageImage() override = default;
  };

  // TODO(dk6): maybe integrate part of funcitonality in base class Image
  // TODO(dk6): derive from StorageImage
  class PyramidImage : public StorageImage {
  private:
    // 13 is up to 8Kx8K texture
    SmallVector<vk::ImageView, 13> _image_views; // these are not Unique to be destroyed before _image

  public:
    PyramidImage(const VulkanState &state, Extent extent, vk::Format format, uint32_t mip_levels_number);
    PyramidImage(PyramidImage&&) noexcept = default;
    PyramidImage & operator=(PyramidImage&&) noexcept = default;
    ~PyramidImage() override = default;

    vk::ImageView get_level(uint32_t level) const noexcept;
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

  constexpr static inline uint32_t calculate_mips_levels_number(Extent extent)
  {
    uint32_t level = 1;
    while (extent.width > 1 && extent.height > 1) {
      extent.width /= 2;
      extent.height = 2;
      level++;
    }
    return level;
  }
}
} // namespace mr

#endif // __MR_IMAGE_HPP_
