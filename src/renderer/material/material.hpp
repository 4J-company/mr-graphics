#ifndef __material_cpp_
#define __material_cpp_

#include "pch.hpp"

#include "manager/manager.hpp"
#include "manager/resource.hpp"

namespace mr {
inline namespace graphics {
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

  class Material : public ResourceBase<Material> {
  public:
    Material(const VulkanState &state,
             const RenderContext &render_context,
             mr::ShaderHandle shader,
             std::span<std::byte> ubo_data,
             std::span<std::optional<mr::TextureHandle>> textures,
             mr::UniformBuffer &cam_ubo) noexcept;

    void bind(CommandUnit &unit) const noexcept;

  protected:
    mr::UniformBuffer _ubo;
    mr::ShaderHandle _shader;

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
        .binding = 1,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = 0
      },
      vk::VertexInputAttributeDescription {
        .location = 2,
        .binding = 2,
        .format = vk::Format::eR32G32Sfloat,
        .offset = 0
      },
    };
  };

  MR_DECLARE_HANDLE(Material)

  class MaterialBuilder {
  private:
    const mr::VulkanState *_state {};
    const mr::RenderContext *_context {};

    std::vector<byte> _specialization_data;
    std::unordered_map<std::string, std::string> _defines;
    std::vector<std::byte> _ubo_data;
    InplaceVector<std::optional<mr::TextureHandle>, enum_cast(MaterialParameter::EnumSize)> _textures;
    mr::UniformBuffer *_cam_ubo;

    std::string_view _shader_filename;

  public:
    MaterialBuilder(
        const mr::VulkanState &state,
        const mr::RenderContext &context,
        std::string_view filename)
      : _state(&state)
      , _context(&context)
      , _shader_filename(filename)
    {
      _textures.resize(_textures.capacity());
    }

    MaterialBuilder(MaterialBuilder &&) noexcept = default;
    MaterialBuilder & operator=(MaterialBuilder &&) noexcept = default;

    MaterialBuilder &add_texture(MaterialParameter param,
                                 const mr::importer::TextureData &tex_data,
                                 math::Color factor = {1.0, 1.0, 1.0, 1.0})
    {
      mr::TextureHandle tex = ResourceManager<Texture>::get().create(mr::unnamed,
        *_state,
        (const std::byte*)tex_data.image.pixels.get(),
        mr::Extent{ tex_data.image.width, tex_data.image.height },
        vk::Format::eR8G8B8A8Srgb
      );

      _textures[enum_cast(param)] = std::move(tex);
      add_value(factor);
      return *this;
    }

    MaterialBuilder & add_value(const auto &value)
    {
      add_span(std::span<std::remove_reference_t<decltype(value)>>{&value, 1});
      return *this;
    }

    template <typename T>
    MaterialBuilder & add_span(std::span<T> span)
    {
      auto bytes = std::as_bytes(span);
      _ubo_data.insert(_ubo_data.end(), bytes.begin(), bytes.end());
      return *this;
    }

    template<typename T, size_t N>
    MaterialBuilder & add_value(Vec<T, N> value)
    {
      add_span(std::span<T>{&value[0], N});
      return *this;
    }

    MaterialBuilder & add_value(float value)
    {
      add_span(std::span<float>{&value, 1});
      return *this;
    }

    MaterialBuilder & add_camera(mr::UniformBuffer &cam_ubo)
    {
      _cam_ubo = &cam_ubo;
      return *this;
    }

    MaterialHandle build() noexcept
    {
      auto &mtlmanager = ResourceManager<Material>::get();
      auto &shdmanager = ResourceManager<Shader>::get();

      auto definestr = _generate_shader_defines_str();
      auto shdname = std::string(_shader_filename) + ":" + definestr;
      auto shdfindres = shdmanager.find(shdname);
      auto shdhandle = shdfindres ? shdfindres : shdmanager.create(shdname, *_state, _shader_filename, _generate_shader_defines());

      ASSERT(_state != nullptr);
      ASSERT(_context != nullptr);

      return mtlmanager.create(unnamed,
        *_state,
        *_context,
        shdhandle,
        std::span {_ubo_data},
        std::span {_textures},
        *_cam_ubo
      );
    }

  private:
    std::string _generate_shader_defines_str() const noexcept
    {
      std::unordered_map<std::string, std::string> defines = _generate_shader_defines();
      std::stringstream ss;
      for (auto &[name, value] : defines) {
        ss << "-D" << name << '=' << value << ' ';
      }
      return ss.str();
    }

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
  };
}
} // namespace mr

#endif // __material_cpp_
