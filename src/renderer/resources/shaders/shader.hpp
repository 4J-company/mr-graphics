#if !defined(__shader_hpp_)
#define __shader_hpp_

#include "pch.hpp"

#include "vulkan_application.hpp"

namespace mr {
  class Shader {
    private:
      static inline const size_t max_shader_modules = 6;

      std::filesystem::path _path;
      std::array<vk::UniqueShaderModule, max_shader_modules> _modules;
      std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> _stages;
      std::atomic<uint> _num_of_loaded_shaders;

      enum struct ShaderStages {
        compute = 0,
        vertex = 1,
        control = 2,
        evaluate = 3,
        geometry = 4,
        fragment = 5,
      };

    public:
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
      void compile(ShaderStages stage);

      // load sources
      std::vector<char> load(ShaderStages stage);

    public:
      std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> &
      get_stages()
      {
        return _stages;
      }

      uint stage_number() { return _num_of_loaded_shaders; }
  };
} // namespace mr

#endif // __shader_hpp_
