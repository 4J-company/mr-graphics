#ifndef __MR_SAMPLER_HPP_
#define __MR_SAMPLER_HPP_

#include "pch.hpp"
#include "vulkan_state.hpp"

namespace mr {
inline namespace graphics {
  class Sampler {
  private:
    vk::UniqueSampler _sampler;

    int _mip_levels_number;
    vk::Filter _filter;
    vk::SamplerAddressMode _address;
    vk::SamplerMipmapMode _mip_map_mode;

  public:
    Sampler() = default;

    Sampler(const VulkanState &state, vk::Filter filter, vk::SamplerMipmapMode mip_map_mode,
            vk::SamplerAddressMode address, int mip_level = 1);

    Sampler(const VulkanState &state, vk::Filter filter, vk::SamplerMipmapMode mip_map_mode,
            vk::SamplerAddressMode address, vk::SamplerReductionMode reduction_mode, int mip_level = 1);

    const vk::Sampler sampler() const { return _sampler.get(); }
  };
}
} // namespace mr

#endif // __MR_SAMPLER_HPP_
