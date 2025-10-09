#ifndef __MR_TEXTURE_HPP_
#define __MR_TEXTURE_HPP_

#include "resources/texture/sampler/sampler.hpp"

#include "manager/resource.hpp"

namespace mr {
inline namespace graphics {
  class Texture : public ResourceBase<Texture> {
    private:
      TextureImage _image;
      Sampler _sampler;

    public:
      Texture(Texture&&) = default;
      Texture & operator=(Texture&&) = default;

      Texture(const VulkanState &state, const std::byte *data, Extent extent, vk::Format format) noexcept;
      Texture(const VulkanState &state, const mr::importer::ImageData &image) noexcept;

      const TextureImage &image() const { return _image; }

      const Sampler &sampler() const { return _sampler; }
  };

  MR_DECLARE_HANDLE(Texture)
}
} // namespace mr

#endif // __MR_TEXTURE_HPP_
