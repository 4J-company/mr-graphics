#include "resources/texture/texture.hpp"
#include "stb/stb_image.h"

/**
 * @brief Constructs a Texture by loading image data from a file.
 *
 * This constructor loads an image from the given filename using stb_image, converting the image data to a 4-channel format 
 * (STBI_rgb_alpha). It then initializes the Texture with the loaded data, its dimensions, and a fixed Vulkan format 
 * (vk::Format::eR8G8B8A8Srgb). If the image data fails to load, an assertion is triggered. The image data is freed immediately 
 * after initializing the Texture.
 *
 * @param filename Path to the image file.
 */
mr::Texture::Texture(const VulkanState &state, std::string_view filename) noexcept
{
  int w, h, ch;
  byte *data = stbi_load(filename.data(), &w, &h, &ch, STBI_rgb_alpha);
  assert(data);

  *this = Texture(state, data, {static_cast<uint>(w), static_cast<uint>(h)}, vk::Format::eR8G8B8A8Srgb);

  stbi_image_free(data);
}

/**
 * @brief Constructs a Texture object from raw image data.
 *
 * This constructor creates an image with the specified dimensions and Vulkan format,
 * writes the provided raw data into it, and updates the image layout to be shader-readable.
 * A default sampler is also initialized using linear filtering with repeat addressing mode.
 *
 * @param data Pointer to the raw image data.
 * @param extent The dimensions of the image.
 * @param format Vulkan format that specifies how the image data is interpreted.
 */
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
