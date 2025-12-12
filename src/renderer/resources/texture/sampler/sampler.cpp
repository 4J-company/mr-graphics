#include "resources/texture/sampler/sampler.hpp"

mr::Sampler::Sampler(const VulkanState &state, vk::Filter filter, vk::SamplerMipmapMode mip_map_mode,
                     vk::SamplerAddressMode address, int mip_level)
  : _filter(filter)
  , _address(address)
  , _mip_map_mode(mip_map_mode)
  , _mip_levels_number(mip_level)
{
  _sampler = create_sampler(state);
}

mr::Sampler::Sampler(const VulkanState &state, vk::Filter filter, vk::SamplerMipmapMode mip_map_mode,
                     vk::SamplerAddressMode address, vk::SamplerReductionMode reduction_mode, int mip_level)
  : _filter(filter)
  , _address(address)
  , _mip_map_mode(mip_map_mode)
  , _mip_levels_number(mip_level)
{
  vk::SamplerReductionModeCreateInfo reduction_info {
    .reductionMode = reduction_mode,
  };

  _sampler = create_sampler(state, &reduction_info);
}

vk::UniqueSampler mr::Sampler::create_sampler(const VulkanState &state, const void *pNext) const noexcept
{
  static auto props = state.phys_device().getProperties();

  vk::SamplerCreateInfo sampler_create_info {
    .pNext = pNext,
    .magFilter = _filter,
    .minFilter = _filter,
    .mipmapMode = _mip_map_mode,
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
  return state.device().createSamplerUnique(sampler_create_info).value;
}
