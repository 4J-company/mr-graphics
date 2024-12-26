#ifndef __MR_TEXTURE_HPP_
#define __MR_TEXTURE_HPP_

#include "resources/texture/sampler/sampler.hpp"

namespace mr {
  class Texture {
    private:
      Image _image;
      Sampler _sampler;

    public:
      Texture() = default;

      Texture(const VulkanState &state, std::string_view filename);

      const Image &image() const { return _image; }

      const Sampler &sampler() const { return _sampler; }
  };
} // namespace mr

#endif // __MR_TEXTURE_HPP_
