#if !defined(__sampler_hpp_)
  #define __sampler_hpp_

  #include "pch.hpp"

namespace ter
{
  class Sampler
  {
  private:
    vk::Sampler _sampler;

    int _mip_level;
    vk::Filter _filter;
    vk::SamplerAddressMode _address;

  public:
    Sampler() = default;
    ~Sampler();

    Sampler(Sampler &&other) noexcept = default;
    Sampler &operator=(Sampler &&other) noexcept = default;
  };
} // namespace ter

#endif // __sampler_hpp_
