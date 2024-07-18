#include "resources/texture/texture.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

// constructor
mr::Texture::Texture(const VulkanState &state, std::string_view filename) noexcept
{
  int w, h, ch;
  byte *data = stbi_load(filename.data(), &w, &h, &ch, STBI_rgb_alpha);
  assert(data);

  *this = Texture(state, data, w, h, vk::Format::eR8G8B8A8Srgb);

  stbi_image_free(data);
}

mr::Texture::Texture(const VulkanState &state, const byte *data, std::size_t height, std::size_t width, vk::Format format) noexcept
{
  _image = Image(state,
                 width,
                 height,
                 format,
                 vk::ImageUsageFlagBits::eTransferDst |
                   vk::ImageUsageFlagBits::eSampled,
                 vk::ImageAspectFlagBits::eColor);

  // TODO: support float formats
  std::size_t texel_size =
    format == vk::Format::eR8G8B8A8Srgb ? 4
    : format == vk::Format::eR8G8B8Srgb ? 3
    : format == vk::Format::eR8G8B8Srgb ? 2
                                        : 1;

  _image.switch_layout(state, vk::ImageLayout::eTransferDstOptimal);
  _image.write<const byte>(state,
                           std::span {data, width * height * texel_size});
  _image.switch_layout(state, vk::ImageLayout::eShaderReadOnlyOptimal);

  _sampler =
    Sampler(state, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat);
}
