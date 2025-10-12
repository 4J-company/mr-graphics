#include "resources/texture/texture.hpp"

mr::Texture::Texture(const VulkanState &state, const std::byte *data, Extent extent, vk::Format format) noexcept
  : _image (state, extent, format)
  , _sampler (state, vk::Filter::eNearest, vk::SamplerAddressMode::eRepeat)
{
  _image.switch_layout(vk::ImageLayout::eTransferDstOptimal);
  _image.write<const std::byte>(std::span{data, _image.size()});
  _image.switch_layout(vk::ImageLayout::eShaderReadOnlyOptimal);
}

mr::Texture::Texture(const VulkanState &state, const mr::importer::ImageData &image) noexcept
  : _image (state, image)
  , _sampler (state, vk::Filter::eNearest, vk::SamplerAddressMode::eRepeat)
{
  _image.switch_layout(vk::ImageLayout::eTransferDstOptimal);
  _image.write<const std::byte>(image.mips[0]);
  _image.switch_layout(vk::ImageLayout::eShaderReadOnlyOptimal);
}
