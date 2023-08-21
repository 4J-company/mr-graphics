#include "resources/images/image.hpp"

ter::Image::Image(VulkanApplication &va, uint width, uint height, vk::Format format, vk::Image image)
  : _image(image), _size({width, height}), _format(format)
{
  _aspect_flags = vk::ImageAspectFlagBits::eColor; 
  craete_image_view(va);
}

void ter::Image::craete_image_view(VulkanApplication &va)
{
  vk::ImageSubresourceRange range
  {
    .aspectMask = _aspect_flags,
    .baseMipLevel = 0,
    .levelCount = 1,
    .baseArrayLayer = 0,
    .layerCount = 1,
  };

  vk::ImageViewCreateInfo create_info
  {
    .image = _image,
    .viewType = vk::ImageViewType::e2D,
    .format = _format,
    .components = {vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, 
                   vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity},
    .subresourceRange = range
  };
  
  vk::Result result;
  std::tie(result, _image_view) = va.get_device().createImageView(create_info);
  assert(result == vk::Result::eSuccess);
}

void ter::Image::switch_layout(vk::ImageLayout layout) {}

void ter::Image::copy_to_host() const {}

void ter::Image::get_pixel(const vk::Extent2D &coords) const {}
