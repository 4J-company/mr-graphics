#include "resources/images/image.hpp"

mr::Image::Image(const VulkanState &state, uint width, uint height, vk::Format format, vk::Image image)
    : _image(image), _size({width, height}), _format(format)
{
  _aspect_flags = vk::ImageAspectFlagBits::eColor;
  craete_image_view(state);
}

void mr::Image::craete_image_view(const VulkanState &state)
{
  vk::ImageSubresourceRange range {
      .aspectMask = _aspect_flags,
      .baseMipLevel = 0,
      .levelCount = 1,
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

void mr::Image::switch_layout(vk::ImageLayout layout) {}

void mr::Image::copy_to_host() const {}

void mr::Image::get_pixel(const vk::Extent2D &coords) const {}
