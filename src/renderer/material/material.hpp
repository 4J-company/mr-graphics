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

    ASSERT(within(0, defines.size())(enum_cast(param)));
    return defines[enum_cast(param)];
  }

  class Material : public ResourceBase<Material> {
  private:
    mr::UniformBuffer _ubo;
    mr::ShaderHandle _shader;

    mr::GraphicsPipeline _pipeline;

    RenderContext *_context = nullptr;

    // requires for deinitialization
    std::array<std::optional<mr::TextureHandle>, enum_cast(MaterialParameter::EnumSize)> _textures;

    std::array<uint32_t, enum_cast(MaterialParameter::EnumSize)> _textures_ids;
    uint32_t _uniform_buffer_id = -1;

  public:
    Material(RenderContext &render_context,
             mr::ShaderHandle shader,
             std::span<std::byte> ubo_data,
             std::span<std::optional<mr::TextureHandle>> textures,
             std::span<mr::StorageBuffer *> storage_buffers,
             std::span<mr::ConditionalBuffer *> conditional_buffers) noexcept;

    ~Material();

    void bind(CommandUnit &unit) const noexcept;
    uint32_t material_ubo_id() noexcept { return _uniform_buffer_id; }

    const mr::GraphicsPipeline & pipeline() const noexcept { return _pipeline; }
  };

  MR_DECLARE_HANDLE(Material)

  class MaterialBuilder {
  private:
    static inline constexpr int max_attached_buffers = 16;

    const mr::VulkanState *_state {};
    mr::RenderContext *_context {};

    std::vector<std::byte> _specialization_data;
    boost::unordered_map<std::string, std::string> _defines;
    std::vector<std::byte> _ubo_data;
    std::array<std::optional<mr::TextureHandle>, enum_cast(MaterialParameter::EnumSize)> _textures;
    InplaceVector<mr::StorageBuffer *, max_attached_buffers / 2> _storage_buffers;
    InplaceVector<mr::ConditionalBuffer *, max_attached_buffers / 2> _conditional_buffers;
    mr::UniformBuffer *_cam_ubo;

    std::string_view _shader_filename;

  public:
    MaterialBuilder(const mr::VulkanState &state,
                    mr::RenderContext &context,
                    std::string_view filename);

    MaterialBuilder(MaterialBuilder &&) noexcept = default;
    MaterialBuilder & operator=(MaterialBuilder &&) noexcept = default;

    MaterialBuilder & add_texture(MaterialParameter param,
                                 const mr::importer::TextureData &tex_data,
                                 math::Color factor = {1.0, 1.0, 1.0, 1.0});

    MaterialBuilder & add_storage_buffer(mr::StorageBuffer *buffer)
    {
      _storage_buffers.push_back(buffer);
      return *this;
    }

    MaterialBuilder & add_conditional_buffer(mr::ConditionalBuffer *buffer)
    {
      _conditional_buffers.push_back(buffer);
      return *this;
    }

    MaterialBuilder & add_value(const auto *value)
    {
      add_span(std::span<std::remove_pointer_t<decltype(value)>>{value, 1});
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

    MaterialHandle build() noexcept;

  private:
    std::string generate_shader_defines_str() const noexcept
    {
      boost::unordered_map<std::string, std::string> defines = generate_shader_defines();
      std::stringstream ss;
      for (auto &[name, value] : defines) {
        ss << "-D" << name << '=' << value << ' ';
      }
      return ss.str();
    }

    boost::unordered_map<std::string, std::string> generate_shader_defines() const noexcept;
  };
}
} // namespace mr

#endif // __material_cpp_
