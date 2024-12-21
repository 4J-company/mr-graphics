#include "resources/texture/texture.hpp"
#include "stb/stb_image.h"

// constructor
mr::Texture::Texture(const VulkanState &state, std::string_view filename) noexcept
{
  int w, h, ch;
  byte *data = stbi_load(filename.data(), &w, &h, &ch, STBI_rgb_alpha);
  assert(data);

  *this = Texture(state, data, {static_cast<uint>(w), static_cast<uint>(h)}, vk::Format::eR8G8B8A8Srgb);

  stbi_image_free(data);
}

mr::Texture::Texture(const VulkanState &state, const byte *data, Extent extent, vk::Format format) noexcept
{
  _image = Image(state,
                 extent,
                 format,
                 vk::ImageUsageFlagBits::eTransferDst |
                   vk::ImageUsageFlagBits::eSampled,
                 vk::ImageAspectFlagBits::eColor);

  _image.switch_layout(state, vk::ImageLayout::eTransferDstOptimal);
  _image.write<const byte>(state, std::span{data, _image.size()});
  _image.switch_layout(state, vk::ImageLayout::eShaderReadOnlyOptimal);

  _sampler =
    Sampler(state, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat);
}
