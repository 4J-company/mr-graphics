#include "resources/texture/texture.hpp"

mr::Texture::Texture(const VulkanState &state, const std::byte *data, Extent extent, vk::Format format) noexcept
  : _image (state, extent, format)
  , _sampler (state, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat)
{
  _image.switch_layout(vk::ImageLayout::eTransferDstOptimal);
  _image.write<const std::byte>(std::span{data, _image.size()});
  _image.switch_layout(vk::ImageLayout::eShaderReadOnlyOptimal);
}
