#include "resources/texture/texture.hpp"
#include "stb/stb_image.h"

// constructor
mr::Texture::Texture(const VulkanState &state, std::string_view filename) noexcept
  : _sampler (state, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat)
  , _image ([&state, filename]{
      int w, h, ch;
      byte *data = stbi_load(filename.data(), &w, &h, &ch, STBI_rgb_alpha);
      assert(data);

      Extent extent = {static_cast<uint>(w), static_cast<uint>(h)};
      TextureImage tmp = TextureImage(state, extent, vk::Format::eR8G8B8A8Srgb);

      tmp.switch_layout(state, vk::ImageLayout::eTransferDstOptimal);

      tmp.write<const byte>(state, std::span{data, tmp.size()});
      stbi_image_free(data);

      tmp.switch_layout(state, vk::ImageLayout::eShaderReadOnlyOptimal);

      return tmp;
    }())
{
}

mr::Texture::Texture(const VulkanState &state, const byte *data, Extent extent, vk::Format format) noexcept
  : _image (state, extent, format)
  , _sampler (state, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat)
{
  _image.switch_layout(state, vk::ImageLayout::eTransferDstOptimal);
  _image.write<const byte>(state, std::span{data, _image.size()});
  _image.switch_layout(state, vk::ImageLayout::eShaderReadOnlyOptimal);
}
