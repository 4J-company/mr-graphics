#ifndef __MR_LIGHT_RENDER_DATA_HPP_
#define __MR_LIGHT_RENDER_DATA_HPP_

#include "resources/buffer/buffer.hpp"
#include "resources/pipelines/graphics_pipeline.hpp"

namespace mr {
inline namespace graphics {
  enum struct LightType : uint32_t {
    Directional,
    // Spot,
    // Point,
    // Sky,
    Number
  };

  // Light base class forward decalaration
  class Light;
  // Light types forward declarations
  class DirectionalLight;

  template <std::derived_from<Light> L>
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

    VertexBuffer screen_vbuf {};
    IndexBuffer screen_ibuf {};

    DescriptorSetLayoutHandle lights_set_layout;
    DescriptorSet lights_descriptor_set;

    InplaceVector<ShaderHandle, light_type_number> shaders {};
    InplaceVector<GraphicsPipeline, light_type_number> pipelines {};

    LightsRenderData() = default;

    LightsRenderData & operator=(LightsRenderData &&) noexcept = default;
    LightsRenderData(LightsRenderData &&) noexcept = default;
  };
}
} // end of mr namespace

#endif // __MR_LIGHT_RENDER_DATA_HPP_
