#if !defined(__texture_hpp_)
#define __texture_hpp_

#include "resources/texture/sampler/sampler.hpp"

namespace mr {
  class Texture {
    private:
      Image _image;
      Sampler _sampler;

    public:
      Texture() = default;

      Texture(const VulkanState &state, std::string_view filename);

      Texture(Texture &&other) noexcept = default;
      Texture &operator=(Texture &&other) noexcept = default;

      const Image &image() const { return _image; }

      const Sampler &sampler() const { return _sampler; }
  };
} // namespace mr

#endif // __texture_hpp_
