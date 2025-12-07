// TODO(dk6): move this file in descriptor directory

#ifndef __MR_SHADER_RESOURCE_HPP_
#define __MR_SHADER_RESOURCE_HPP_

#include "pch.hpp"
#include "renderer/resources/images/image.hpp"
#include "renderer/resources/texture/sampler/sampler.hpp"
#include "renderer/resources/buffer/buffer.hpp"

namespace mr {
inline namespace graphics {

struct ShaderImageResource {
  const Image *image;
  // if sampler is NULL - type will be StorageImage, else eCombinedImageSampler
  const Sampler *sampler = nullptr;
  // If layout is undefined will be used currect layout of image
  vk::ImageLayout layout = vk::ImageLayout::eUndefined;
};

struct ShaderPyramidImageLevelResource {
  const PyramidImage *image;
  uint32_t mip_level = 0;
  // if sampler is NULL - type will be StorageImage, else eCombinedImageSampler
  const Sampler *sampler = nullptr;
  // If layout is undefined will be used currect layout of image
  vk::ImageLayout layout = vk::ImageLayout::eUndefined;
};

using ShaderResource = std::variant<
  const UniformBuffer *,
  const StorageBuffer *,
  const ShaderImageResource *,
  const ShaderPyramidImageLevelResource *,
  const ColorAttachmentImage *, // It is not supported for bindless descriptor set
  const ConditionalBuffer * // TODO(dk6): maybe remove it
>;

constexpr static uint32_t shader_resource_uniform_buffer_index = 0;
constexpr static uint32_t shader_resource_storage_buffer_index = 1;
constexpr static uint32_t shader_resource_image_index = 2;
constexpr static uint32_t shader_resource_pyramid_image_level_index = 3;
constexpr static uint32_t shader_resource_color_attachment_index = 4;

struct ShaderResourceView {
  uint32_t binding;
  ShaderResource res;

  operator const ShaderResource&() const { return res; }
};

} // namespace graphics
} // namespace mr

#endif // __MR_SHADER_RESOURCE_HPP_
