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

    public:
      Sampler() = default;

      Sampler(const VulkanState &state, vk::Filter filter,
              vk::SamplerAddressMode address, int mip_level = 1);

      const vk::Sampler sampler() const { return _sampler.get(); }
  };
}
} // namespace mr

#endif // __MR_SAMPLER_HPP_
