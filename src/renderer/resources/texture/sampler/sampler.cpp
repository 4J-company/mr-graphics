#include "resources/texture/sampler/sampler.hpp"

mr::Sampler::Sampler(const VulkanState &state, vk::Filter filter,
                     vk::SamplerAddressMode address, int mip_level)
    : _filter(filter)
    , _address(address)
    , _mip_levels_number(mip_level)
{
  static auto props = state.phys_device().getProperties();

  vk::SamplerCreateInfo sampler_create_info {
    .magFilter = _filter,
    .minFilter = _filter,
    .mipmapMode = vk::SamplerMipmapMode::eLinear,
    .addressModeU = _address,
    .addressModeV = _address,
    .addressModeW = _address,
    .mipLodBias = 0.0f,
    .anisotropyEnable = true,
    .maxAnisotropy = props.limits.maxSamplerAnisotropy,
    .compareEnable = false,
    .compareOp = vk::CompareOp::eAlways,
    .minLod = 0.0f,
    .maxLod = static_cast<float>(_mip_levels_number),
    .borderColor = vk::BorderColor::eIntOpaqueBlack,
    .unnormalizedCoordinates = false,
  };
  _sampler = state.device().createSamplerUnique(sampler_create_info).value;
}
