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

      Texture(const VulkanState &state, std::string_view filename) noexcept;
      Texture(const VulkanState &state, const byte *data, std::size_t height, std::size_t width, vk::Format format) noexcept;

      const Image &image() const { return _image; }

      const Sampler &sampler() const { return _sampler; }
  };
} // namespace mr

#endif // __texture_hpp_
