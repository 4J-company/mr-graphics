#if !defined(__shader_hpp_)
#define __shader_hpp_

#include "pch.hpp"

#include "vulkan_application.hpp"

namespace mr {
  class UniformBuffer;
  class StorageBuffer;
  class Texture;
  class Image;

  class Shader {
    private:
      static inline const size_t max_shader_modules = 6;

      std::filesystem::path _path;
      std::array<vk::UniqueShaderModule, max_shader_modules> _modules;
      std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> _stages;
      std::atomic<uint> _num_of_loaded_shaders;

    public:
      using Resource = std::variant<UniformBuffer *, StorageBuffer *, Texture *, Image *>;
      enum struct Stage {
        Compute  = 0,
        Vertex   = 1,
        Control  = 2,
        Evaluate = 3,
        Geometry = 4,
        Fragment = 5,
      };
      struct ResourceView {
        uint32_t set;
        uint32_t binding;
        const Resource &res;

        operator const Resource&() {
          return res;
        }
      };

      Shader() = default;

      Shader(const VulkanState &state, std::string_view filename);

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
      void compile(Stage stage);

      // load sources
      std::optional<std::vector<char>> load(Stage stage);

    public:
      std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> &
      get_stages()
      {
        return _stages;
      }

      uint stage_number() { return _num_of_loaded_shaders; }
  };

  inline constexpr vk::ShaderStageFlagBits get_stage_flags(std::integral auto stage) noexcept
  {
    static constexpr const vk::ShaderStageFlagBits stage_bits[] = {
      vk::ShaderStageFlagBits::eCompute,
      vk::ShaderStageFlagBits::eVertex,
      vk::ShaderStageFlagBits::eTessellationControl,
      vk::ShaderStageFlagBits::eTessellationEvaluation,
      vk::ShaderStageFlagBits::eGeometry,
      vk::ShaderStageFlagBits::eFragment};

    return stage_bits[stage];
  }

  inline constexpr vk::ShaderStageFlagBits get_stage_flags(Shader::Stage stage) noexcept
  {
    return get_stage_flags(std::to_underlying(stage));
  }

  inline constexpr const char * get_stage_name(std::integral auto stage)
  {
    static constexpr const char *shader_type_names[] = {
      "comp",
      "vert",
      "tesc",
      "tese",
      "geom",
      "frag",
    };
    return shader_type_names[stage];
  }

  inline constexpr const char * get_stage_name(Shader::Stage stage)
  {
    return get_stage_name(std::to_underlying(stage));
  }
} // namespace mr

#endif // __shader_hpp_
