#ifndef __MR_SHADER_HPP_
#define __MR_SHADER_HPP_

#include "pch.hpp"

#include "manager/resource.hpp"

#include "vulkan_state.hpp"

namespace mr {
inline namespace graphics {
  class UniformBuffer;
  class StorageBuffer;
  class Texture;
  class Image;

  class Shader : public ResourceBase<Shader> {
    private:
      static inline const size_t max_shader_modules = 6;

      std::filesystem::path _path;
      std::array<vk::UniqueShaderModule, max_shader_modules> _modules;
      std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> _stages;
      std::atomic<uint> _num_of_loaded_shaders;
      std::string _define_string;
      std::string _include_string;

    public:
      using Resource = std::variant<const UniformBuffer *, const StorageBuffer *, const Texture *, const Image *, const ConditionalBuffer*>;

      // TODO: consider RT shaders from extensions;
      // TODO(dk6): remove this enum, use vk::ShaderStageFlagsBits instead
      enum struct Stage {
        Compute  = 0,
        Vertex   = 1,
        Control  = 2,
        Evaluate = 3,
        Geometry = 4,
        Fragment = 5,
      };

      struct ResourceView {
        uint32_t binding;
        Resource res;

        operator const Resource&() const { return res; }
      };

      Shader() = default;

      Shader(const VulkanState &state, std::string_view filename,
             const boost::unordered_map<std::string, std::string> &define_map = {});

      // move semantics
      Shader(Shader &&other) noexcept
          : _path(std::move(other._path))
          , _modules(std::move(other._modules))
          , _stages(std::move(other._stages))
          , _num_of_loaded_shaders(other._num_of_loaded_shaders.load())
      {
      }

      Shader &operator=(Shader &&other) noexcept
      {
        // do not need a self check

        _path = std::move(other._path);
        _modules = std::move(other._modules);
        _stages = std::move(other._stages);
        _num_of_loaded_shaders = other._num_of_loaded_shaders.load();
        return *this;
      }

      // reload shader
      void reload();

    private:
      // compile sources
      void compile(Stage stage) const noexcept;

      // load sources
      std::optional<std::vector<char>> load(Stage stage) noexcept;

      bool _validate_stage(Stage stage, bool present)  const noexcept;

    public:
      const std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> & stages() const { return _stages; }

      std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> & stages() { return _stages; }

      uint stage_number() const noexcept { return _num_of_loaded_shaders; }

      std::string name() const noexcept { return _path.stem(); }
  };

  MR_DECLARE_HANDLE(Shader)

  constexpr vk::ShaderStageFlagBits get_stage_flags(std::integral auto stage) noexcept
  {
    constexpr std::array stage_bits {
      vk::ShaderStageFlagBits::eCompute,
      vk::ShaderStageFlagBits::eVertex,
      vk::ShaderStageFlagBits::eTessellationControl,
      vk::ShaderStageFlagBits::eTessellationEvaluation,
      vk::ShaderStageFlagBits::eGeometry,
      vk::ShaderStageFlagBits::eFragment
    };
    ASSERT(stage < stage_bits.size());

    return stage_bits[stage];
  }

  constexpr vk::ShaderStageFlagBits get_stage_flags(Shader::Stage stage) noexcept
  {
    return get_stage_flags(std::to_underlying(stage));
  }

  constexpr const char * get_stage_name(std::integral auto stage) noexcept
  {
    constexpr std::array shader_type_names {
      "comp",
      "vert",
      "tesc",
      "tese",
      "geom",
      "frag",
    };
    ASSERT(stage < shader_type_names.size());
    return shader_type_names[stage];
  }

  constexpr const char * get_stage_name(Shader::Stage stage) noexcept
  {
    return get_stage_name(std::to_underlying(stage));
  }
}
} // namespace mr

#endif // __MR_SHADER_HPP_
