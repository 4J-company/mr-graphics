#ifndef __material_cpp_
#define __material_cpp_

#include "pch.hpp"
#include "resources/resources.hpp"
#include "utils/misc.hpp"

namespace mr {
  enum class MaterialParameter {
    BaseColor,
    MetallicRoughness,
    EmissiveColor,
    OcclusionMap,
    NormalMap,

    EnumSize
  };

  inline const char * get_material_parameter_define(MaterialParameter param) noexcept {
    constexpr std::array defines {
      "BASE_COLOR_MAP_BINDING",
      "METALLIC_ROUGHNESS_MAP_BINDING",
      "EMISSIVE_MAP_BINDING",
      "OCCLUSION_MAP_BINDING",
      "NORMAL_MAP_BINDING",
    };
    static_assert(defines.size() == enum_cast(MaterialParameter::EnumSize),
      "Shader define is not provided for all MaterialParameter enumerators");

    assert(within(0, defines.size())(enum_cast(param)));
    return defines[enum_cast(param)];
  }

  class Material {
  public:
    Material(const VulkanState &state, const vk::RenderPass render_pass, Shader shader,
             std::span<float> ubo_data, std::span<std::optional<mr::Texture>> textures, mr::UniformBuffer &cam_ubo) noexcept;

    void bind(CommandUnit &unit) const noexcept;

  protected:
    mr::UniformBuffer _ubo;
    mr::Shader _shader;

    mr::DescriptorAllocator _descriptor_allocator;
    mr::DescriptorSet _descriptor_set;
    mr::GraphicsPipeline _pipeline;

    std::array<vk::VertexInputAttributeDescription, 3> _descrs {
      vk::VertexInputAttributeDescription {
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = 0
      },
      vk::VertexInputAttributeDescription {
        .location = 1,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = 3 * sizeof(float)
      },
      vk::VertexInputAttributeDescription {
        .location = 2,
        .binding = 0,
        .format = vk::Format::eR32G32Sfloat,
        .offset = 6 * sizeof(float)
      },
    };
  };

  class MaterialBuilder {
  public:
    MaterialBuilder(
      const mr::VulkanState &state,
      const vk::RenderPass &render_pass,
      std::string_view filename)
        : _state(&state)
        , _render_pass(render_pass)
        , _shader_filename(filename)
    {
      _textures.resize(enum_cast(MaterialParameter::EnumSize));
    }

    MaterialBuilder(MaterialBuilder &&) noexcept = default;
    MaterialBuilder & operator=(MaterialBuilder &&) noexcept = default;

    MaterialBuilder &add_texture(
      MaterialParameter param,
      std::optional<mr::Texture> tex,
      Color factor = {1.0, 1.0, 1.0, 1.0})
    {
      _textures[enum_cast(param)] = std::move(tex);
      add_value(factor);
      return *this;
    }

    template<typename T, size_t N>
    MaterialBuilder & add_value(Vec<T, N> value)
    {
      for (size_t i = 0; i < N; i++) {
        _ubo_data.push_back(value[i]);
      }
      return *this;
    }

    MaterialBuilder & add_value(const auto &value)
    {
#ifdef __cpp_lib_containers_ranges
      _ubo_data.append_range(std::span {(float *)&value, sizeof(value) / sizeof(float)});
#else
      _ubo_data.insert(_ubo_data.end(), (float *)&value, (float *)(&value + 1));
#endif
      return *this;
    }

    MaterialBuilder & add_value(float value)
    {
      _ubo_data.push_back(value);
      return *this;
    }

    MaterialBuilder & add_camera(mr::UniformBuffer &cam_ubo)
    {
      _cam_ubo = &cam_ubo;
      return *this;
    }

    Material build() noexcept
    {
      return Material {
        *_state,
        _render_pass,
        mr::Shader(*_state,
                   _shader_filename,
                   _generate_shader_defines()),
        std::span {_ubo_data},
        std::span {_textures},
        *_cam_ubo
      };
    }

  private:
    std::unordered_map<std::string, std::string> _generate_shader_defines() const noexcept
    {
      std::unordered_map<std::string, std::string> defines;
      for (size_t i = 0; i < enum_cast(MaterialParameter::EnumSize); i++) {
        if (_textures[i].has_value()) {
          defines[get_material_parameter_define(enum_cast<MaterialParameter>(i))] = std::to_string(i + 2);
        }
      }
      return defines;
    }

    const mr::VulkanState *_state{nullptr};
    vk::RenderPass _render_pass;

    std::vector<byte> _specialization_data;
    std::unordered_map<std::string, std::string> _defines;
    std::vector<float> _ubo_data;
    std::vector<std::optional<mr::Texture>> _textures;
    mr::UniformBuffer *_cam_ubo;

    std::string_view _shader_filename;
  };
} // namespace mr

#endif // __material_cpp_
