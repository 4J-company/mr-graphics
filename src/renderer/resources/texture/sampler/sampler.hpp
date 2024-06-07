#if !defined(__sampler_hpp_)
#define __sampler_hpp_

#include "pch.hpp"
#include "vulkan_application.hpp"

namespace mr {
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

      Sampler(Sampler &&other) noexcept = default;
      Sampler &operator=(Sampler &&other) noexcept = default;

      const vk::Sampler sampler() const { return _sampler.get(); }
  };
} // namespace mr

#endif // __sampler_hpp_
