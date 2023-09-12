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

    vk::Extent2D _size;
    vk::Format _format;
    int _num_of_channels;
    int _mip_level;

    vk::ImageUsageFlags _usage_flags;
    vk::ImageAspectFlags _aspect_flags;

  public:
    Image() = default;
    ~Image() = default;

    Image(Image &&other) = default;
    Image &operator=(Image &&other) = default;

    Image(const VulkanState &state, uint width, uint height, vk::Format format, vk::Image image);

    void switch_layout(vk::ImageLayout layout);
    void copy_to_host() const;
    void get_pixel(const vk::Extent2D &coords) const;

  private:
    void craete_image_view(const VulkanState &state);

  public:
    const vk::ImageView & image_view() const { return _image_view.get(); }
  };
} // namespace mr

#endif // __image_hpp_
