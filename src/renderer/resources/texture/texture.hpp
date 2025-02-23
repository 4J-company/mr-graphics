#ifndef __MR_TEXTURE_HPP_
#define __MR_TEXTURE_HPP_

#include "resources/texture/sampler/sampler.hpp"
#include "manager/resource.hpp"

namespace mr {
  class Texture : public ResourceBase<Texture> {
    private:
      Image _image;
      Sampler _sampler;

    public:
      /**
 * @brief Constructs a Texture object with default initialization.
 *
 * This default constructor creates a Texture instance with its image and sampler
 * members default-initialized. No additional resource loading or setup is performed.
 */
Texture() = default;

      Texture(const VulkanState &state, std::string_view filename) noexcept;
      Texture(const VulkanState &state, const byte *data, Extent extent, vk::Format format) noexcept;

      /**
 * @brief Returns a constant reference to the texture's image.
 *
 * Provides access to the underlying image resource associated with the texture.
 *
 * @return const Image& Reference to the texture's image.
 */
const Image &image() const { return _image; }

      /**
 * @brief Retrieves the texture's sampler.
 *
 * Returns a constant reference to the sampler used to control how the texture is sampled during rendering.
 *
 * @return A constant reference to the texture's Sampler.
 */
const Sampler &sampler() const { return _sampler; }
  };

  MR_DECLARE_HANDLE(Texture)
} // namespace mr

#endif // __MR_TEXTURE_HPP_
