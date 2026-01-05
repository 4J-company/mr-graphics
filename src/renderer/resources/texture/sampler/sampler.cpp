#include "resources/texture/sampler/sampler.hpp"


static vk::UniqueSampler create_sampler(const mr::VulkanState &state, vk::Filter filter,
                                        vk::SamplerMipmapMode mip_map_mode, vk::SamplerAddressMode address,
                                        int mip_level, const void *pNext = nullptr)
{
  static auto props = state.phys_device().getProperties();

  vk::SamplerCreateInfo sampler_create_info {
    .pNext = pNext,
    .magFilter = filter,
    .minFilter = filter,
    .mipmapMode = mip_map_mode,
    .addressModeU = address,
    .addressModeV = address,
    .addressModeW = address,
    .mipLodBias = 0.0f,
    .anisotropyEnable = true,
    .maxAnisotropy = props.limits.maxSamplerAnisotropy,
    .compareEnable = false,
    .compareOp = vk::CompareOp::eAlways,
    .minLod = 0.0f,
    .maxLod = static_cast<float>(mip_level),
    .borderColor = vk::BorderColor::eIntOpaqueBlack,
    .unnormalizedCoordinates = false,
  };
  return state.device().createSamplerUnique(sampler_create_info).value;

}

mr::Sampler::Sampler(const VulkanState &state, vk::Filter filter, vk::SamplerMipmapMode mip_map_mode,
                     vk::SamplerAddressMode address, int mip_level)
  : _filter(filter)
  , _address(address)
  , _mip_map_mode(mip_map_mode)
  , _mip_levels_number(mip_level)
{
  _sampler = create_sampler(state, filter, mip_map_mode, address, mip_level);
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

  _sampler = create_sampler(state, filter, mip_map_mode, address, mip_level, &reduction_info);
}
