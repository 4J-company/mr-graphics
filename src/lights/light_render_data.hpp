#ifndef __MR_LIGHT_RENDER_DATA_HPP_
#define __MR_LIGHT_RENDER_DATA_HPP_

#include "resources/buffer/buffer.hpp"
#include "resources/pipelines/graphics_pipeline.hpp"
#include "beman/inplace_vector/inplace_vector.hpp"

namespace mr {
  enum struct LightType : uint32_t {
    Directional,
    // Spot,
    // Point,
    // Sky,
    Number
  };

  // Light types forward declarations
  class DirectionalLight;

  template <typename L>
  inline constexpr uint32_t get_light_type(void) noexcept {
    using ClearLightT = std::decay_t<L>;
    if constexpr (std::is_same_v<ClearLightT, DirectionalLight>) {
      return std::to_underlying(LightType::Directional);
    } else {
      static_assert(false, "Unknown light type");
    }
  }

  struct LightsRenderData {
    static constexpr uint32_t light_type_number = std::to_underlying(LightType::Number);

    static inline constexpr std::array<std::string_view, light_type_number> shader_names {
      "light", // LightType::Directional
    };

    static constexpr uint32_t max_resource_number = 2;
    using ShaderResourceDescriptionT = beman::inplace_vector<Shader::ResourceView, max_resource_number>;
    static inline std::array<ShaderResourceDescriptionT, light_type_number> shader_resources_descriptions {
      ShaderResourceDescriptionT { // LightType::Directional
        // We want just describe layout of descriptor sets, data isn't required here
        Shader::ResourceView(1, 0, static_cast<UniformBuffer *>(nullptr)),
      },
    };

    VertexBuffer screen_vbuf {};
    IndexBuffer screen_ibuf {};

    std::optional<DescriptorAllocator> set0_descriptor_allocator;
    DescriptorSetLayoutHandle set0_layout;
    DescriptorSet set0_set;

    // Here will be light UBO and shadow map
    beman::inplace_vector<DescriptorAllocator, light_type_number> set1_descriptor_allocators {};
    beman::inplace_vector<DescriptorSetLayoutHandle, light_type_number> set1_layouts {};

    beman::inplace_vector<ShaderHandle, light_type_number> shaders {};
    beman::inplace_vector<GraphicsPipeline, light_type_number> pipelines {};

    // TODO(dk6): maybe use bindless rendering and add here descriptor set and SSBO for each types

    LightsRenderData() = default;

    LightsRenderData & operator=(LightsRenderData &&) noexcept = default;
    LightsRenderData(LightsRenderData &&) noexcept = default;
  };
} // end of mr namespace

#endif // __MR_LIGHT_RENDER_DATA_HPP_