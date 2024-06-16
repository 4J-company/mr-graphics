module;
#include "pch.hpp"
export module Sampler;

import VulkanApplication;

export namespace mr {
  class Sampler {
    private:
      vk::UniqueSampler _sampler;

      int _mip_level;
      vk::Filter _filter;
      vk::SamplerAddressMode _address;

    public:
      Sampler() = default;

      Sampler(const VulkanState &state, vk::Filter filter,
              vk::SamplerAddressMode address, int mip_level = 1);

      const vk::Sampler sampler() const { return _sampler.get(); }
  };
}

mr::Sampler::Sampler(const VulkanState &state, vk::Filter filter,
                     vk::SamplerAddressMode address, int mip_level)
    : _filter(filter)
    , _address(address)
    , _mip_level(mip_level)
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
    .maxLod = static_cast<float>(_mip_level),
    .borderColor = vk::BorderColor::eIntOpaqueBlack,
    .unnormalizedCoordinates = false,
  };
  _sampler = state.device().createSamplerUnique(sampler_create_info).value;
}
