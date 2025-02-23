#ifndef __material_cpp_
#define __material_cpp_

#include "pch.hpp"
#include "resources/resources.hpp"
#include "utils/misc.hpp"
#include "manager/manager.hpp"

namespace mr {
  enum class MaterialParameter {
    BaseColor,
    MetallicRoughness,
    EmissiveColor,
    OcclusionMap,
    NormalMap,

    EnumSize
  };

  /**
   * @brief Retrieves the shader define string for a material parameter.
   *
   * Maps a given MaterialParameter enumeration value to its corresponding shader define string literal.
   * A static assertion ensures that every material parameter has an associated shader define.
   * The function asserts that the provided parameter is within the valid range.
   *
   * @param param The material parameter enumeration value.
   * @return const char* The shader define string corresponding to the specified material parameter.
   */
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
    Material(const VulkanState &state, const vk::RenderPass render_pass, mr::ShaderHandle shader,
             std::span<float> ubo_data, std::span<std::optional<mr::TextureHandle>> textures, mr::UniformBuffer &cam_ubo) noexcept;

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
  public:
    /**
     * @brief Constructs a MaterialBuilder for creating Material resources.
     *
     * Initializes the builder with a reference to the Vulkan state, a render pass, and the shader filename.
     * Also resizes the internal textures container to match the number of material parameters.
     *
     * @param filename Shader filename used for material shader creation.
     */
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

    /**
 * @brief Move constructs a MaterialBuilder instance.
 *
 * Transfers ownership of internal resources from the source MaterialBuilder to a new instance.
 * This move constructor is marked as noexcept.
 */
MaterialBuilder(MaterialBuilder &&) noexcept = default;
    /**
 * @brief Move assignment operator for MaterialBuilder.
 *
 * Transfers ownership of resources from the given MaterialBuilder instance using move semantics.
 * The source instance is left in a valid but unspecified state.
 * This operation is marked as noexcept.
 *
 * @return MaterialBuilder& Reference to the current instance.
 */
MaterialBuilder & operator=(MaterialBuilder &&) noexcept = default;

    /**
     * @brief Adds a texture and its corresponding color factor to the material.
     *
     * Associates an optional texture with a specific material parameter and appends its color factor to the uniform data.
     * If a texture handle is provided, it must be valid (non-null). This method supports chaining.
     *
     * @param param Material parameter slot to assign the texture.
     * @param tex Optional texture handle for the specified parameter.
     * @param factor Optional color factor to modulate the texture; defaults to white (1.0, 1.0, 1.0, 1.0).
     * @return Reference to the MaterialBuilder instance for method chaining.
     */
    MaterialBuilder &add_texture(
      MaterialParameter param,
      std::optional<mr::TextureHandle> tex,
      Color factor = {1.0, 1.0, 1.0, 1.0})
    {
      assert(!tex.has_value() || *tex != nullptr);
      _textures[enum_cast(param)] = std::move(tex);
      add_value(factor);
      return *this;
    }

    template<typename T, size_t N>
    /**
     * @brief Appends vector elements to the uniform buffer data.
     *
     * Iterates through each component of the provided vector and appends it to the internal uniform buffer data,
     * enabling the inclusion of various material parameter values.
     *
     * @tparam T The type of the vector elements.
     * @tparam N The number of elements in the vector.
     * @param value A vector containing the values to add to the uniform buffer.
     * @return Reference to the current MaterialBuilder instance.
     */
    MaterialBuilder & add_value(Vec<T, N> value)
    {
      for (size_t i = 0; i < N; i++) {
        _ubo_data.push_back(value[i]);
      }
      return *this;
    }

    /**
     * @brief Appends an arbitrary value to the uniform buffer data.
     *
     * This method adds the raw memory representation of the provided value, interpreted as a series of floats,
     * to the uniform buffer data. This facilitates the flexible inclusion of various types of data in the material's
     * uniform buffer.
     *
     * @param value The value to be appended, which should be a POD type safely interpretable as an array of floats.
     * @return Reference to the current MaterialBuilder instance for chaining.
     *
     * @note The function interprets the provided value's memory layout as a float array and its size is determined
     * by the value's total byte size divided by the size of a float.
     */
    MaterialBuilder & add_value(const auto &value)
    {
#ifdef __cpp_lib_containers_ranges
      _ubo_data.append_range(std::span {(float *)&value, sizeof(value) / sizeof(float)});
#else
      _ubo_data.insert(_ubo_data.end(), (float *)&value, (float *)(&value + 1));
#endif
      return *this;
    }

    /**
     * @brief Appends a float value to the uniform buffer data.
     *
     * This method adds the provided float value to the internal buffer used for
     * storing uniform data, facilitating the configuration of shader parameters.
     * It returns a reference to the current MaterialBuilder instance to allow method chaining.
     *
     * @param value The float value to be added.
     * @return MaterialBuilder& Reference to the current MaterialBuilder instance.
     */
    MaterialBuilder & add_value(float value)
    {
      _ubo_data.push_back(value);
      return *this;
    }

    /**
     * @brief Associates a camera uniform buffer with the material builder.
     *
     * This method sets the camera uniform buffer for the builder, which will be used when constructing
     * the material to pass camera-related parameters.
     *
     * @param cam_ubo A reference to the camera uniform buffer.
     * @return MaterialBuilder& Reference to the builder instance to enable method chaining.
     */
    MaterialBuilder & add_camera(mr::UniformBuffer &cam_ubo)
    {
      _cam_ubo = &cam_ubo;
      return *this;
    }

    /**
     * @brief Builds and returns a Material resource based on the current builder configuration.
     *
     * This method generates a unique shader identifier by combining the shader filename with
     * dynamically produced shader defines. It retrieves an existing shader from the shader manager
     * or creates a new one if not found, then constructs a Material resource using the provided
     * Vulkan state, render pass, uniform buffer data, textures, and camera uniform buffer.
     *
     * @return MaterialHandle Handle to the newly created Material resource.
     */
    MaterialHandle build() noexcept
    {
      auto &mtlmanager = ResourceManager<Material>::get();

      auto &shdmanager = ResourceManager<Shader>::get();
      auto definestr = _generate_shader_defines_str();
      auto shdname = std::string(_shader_filename) + ":" + definestr;
      auto shdfindres = shdmanager.find(shdname);
      auto shdhandle = shdfindres ? shdfindres : shdmanager.create(shdname, *_state, _shader_filename, _generate_shader_defines());

      return mtlmanager.create(unnamed,
        *_state,
        _render_pass,
        shdhandle,
        std::span {_ubo_data},
        std::span {_textures},
        *_cam_ubo
      );
    }

  private:
    /**
     * @brief Generates a space-separated string of shader define flags.
     *
     * This method constructs a string in which each shader definition is formatted as
     * "-D<name>=<value>" and concatenated with a trailing space. The definitions are obtained
     * from the internal shader defines map provided by _generate_shader_defines(), making the result
     * suitable for use as command-line flags or similar shader configurations.
     *
     * @return std::string A single string containing all shader define flags.
     */
    std::string _generate_shader_defines_str() const noexcept
    {
      std::unordered_map<std::string, std::string> defines = _generate_shader_defines();
      std::stringstream ss;
      for (auto &[name, value] : defines) {
        ss << "-D" << name << '=' << value << ' ';
      }
      return ss.str();
    }

    /**
     * @brief Generates shader define mappings for available textures.
     *
     * Iterates over all material parameters and, for each parameter with an associated texture,
     * maps the corresponding shader define string (obtained via get_material_parameter_define) to a
     * texture unit index, represented as a string (computed as the parameter's index plus two).
     *
     * @return An unordered map where keys are shader define strings and values are texture unit indices as strings.
     */
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
    std::vector<std::optional<mr::TextureHandle>> _textures;
    mr::UniformBuffer *_cam_ubo;

    std::string_view _shader_filename;
  };
} // namespace mr

#endif // __material_cpp_
