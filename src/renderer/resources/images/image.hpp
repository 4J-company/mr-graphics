#if !defined(__image_hpp_)
  #define __image_hpp_

  #include "pch.hpp"

namespace ter
{
  class Image
  {
  private:
    vk::Image _image;
    vk::ImageView _image_view;

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

    Image(std::string_view filename);
    Image(vk::Image image);

    void switch_layout(vk::ImageLayout layout);
    void copy_to_host() const;
    void get_pixel(const vk::Extent2D &coords) const;
  };
} // namespace ter

#endif // __image_hpp_
