#include "resources/texture/sampler/sampler.hpp"
#if !defined(__descriptor_hpp_)
  #define __descriptor_hpp_

  #include "pch.hpp"
  #include "resources/attachment/attachment.hpp"
  #include "resources/images/image.hpp"
  #include "resources/texture/sampler/sampler.hpp"

namespace ter
{
  class Descriptor
  {
  private:
    inline static const int _max_sets = 4;

    vk::DescriptorPool _pool;
    std::array<vk::DescriptorSet, _max_sets> _sets;
    std::array<vk::DescriptorSetLayout, _max_sets> _set_layouts;

  public:
    Descriptor() = default;
    ~Descriptor() = default;

    Descriptor(Descriptor &&other) noexcept = default;
    Descriptor &operator=(Descriptor &&other) noexcept = default;

    void apply();
  };
} // namespace ter

#endif __descriptor_hpp_
