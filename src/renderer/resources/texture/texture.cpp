#include "resources/texture/texture.hpp"

mr::Texture::Texture(const VulkanState &state, const std::byte *data, Extent extent, vk::Format format) noexcept
  : _image(state, extent, format)
  , _sampler(state, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat)
{
  CommandUnit command_unit {state};
  command_unit.begin();

  _image.switch_layout(command_unit, vk::ImageLayout::eTransferDstOptimal);
  _image.write<const std::byte>(command_unit, std::span{data, _image.size()});
  _image.switch_layout(command_unit, vk::ImageLayout::eShaderReadOnlyOptimal);

  command_unit.end();

  UniqueFenceGuard(state.device(), command_unit.submit(state));
}

mr::Texture::Texture(const VulkanState &state,
  const mr::importer::ImageData &image,
  const mr::importer::SamplerData &sampler) noexcept
  : _image(state, image)
  , _sampler(state, sampler.mag, vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat)
{
  CommandUnit command_unit {state};
  command_unit.begin();

  _image.switch_layout(command_unit, vk::ImageLayout::eTransferDstOptimal);
  _image.write<const std::byte>(command_unit, image.mips[0]);
  _image.switch_layout(command_unit, vk::ImageLayout::eShaderReadOnlyOptimal);

  command_unit.end();

  UniqueFenceGuard(state.device(), command_unit.submit(state));
}
