#include "resources/texture/texture.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

// constructor
mr::Texture::Texture(const VulkanState &state, std::string_view filename)
{
  int w, h, ch;
  byte *data = stbi_load(filename.data(), &w, &h, &ch, STBI_rgb_alpha);
  assert(data);

  _image = Image(state,
                 {static_cast<uint>(w), static_cast<uint>(h)},
                 vk::Format::eR8G8B8A8Srgb,
                 vk::ImageUsageFlagBits::eTransferDst |
                   vk::ImageUsageFlagBits::eSampled,
                 vk::ImageAspectFlagBits::eColor);
  _image.switch_layout(state, vk::ImageLayout::eTransferDstOptimal);
  _image.write<const byte>(state,
                           std::span{data, _image.size()});
  _image.switch_layout(state, vk::ImageLayout::eShaderReadOnlyOptimal);

  _sampler =
    Sampler(state, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat);

  stbi_image_free(data);
}
