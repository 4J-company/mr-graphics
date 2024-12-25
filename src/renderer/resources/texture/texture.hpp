#if !defined(__texture_hpp_)
#define __texture_hpp_

#include "resources/texture/sampler/sampler.hpp"
#include "manager/resource.hpp"

namespace mr {
  class Texture : public ResourceBase<Texture> {
    private:
      Image _image;
      Sampler _sampler;

    public:
      Texture() = default;

      Texture(const VulkanState &state, std::string_view filename) noexcept;
      Texture(const VulkanState &state, const byte *data, Extent extent, vk::Format format) noexcept;

      const Image &image() const { return _image; }

      const Sampler &sampler() const { return _sampler; }
  };

  MR_DECLARE_HANDLE(Texture)
} // namespace mr

#endif // __texture_hpp_
