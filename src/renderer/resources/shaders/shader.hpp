#if !defined(__shader_hpp_)
#define __shader_hpp_

#include "pch.hpp"

#include "vulkan_application.hpp"

namespace ter
{
  class Shader
  {
    inline static const size_t max_shader_modules = 6;

    std::string _path;
    std::array<vk::ShaderModule, max_shader_modules> _modules;
    std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> _stages;
    std::atomic<uint> _num_of_loaded_shaders;

    enum struct ShaderStages : size_t
    {
      COMPUTE = 0,
      VERTEX = 1,
      CONTROL = 2,
      EVALUATE = 3,
      GEOMETRY = 4,
      FRAGMENT = 5,
    };

  public:
    Shader() = default;
    ~Shader() = default;

    Shader(VulkanApplication &va, std::string_view filename);

    // move semantics
    Shader(Shader &&other) noexcept
    {
      std::swap(_path, other._path);
      std::swap(_modules, other._modules);
      std::swap(_stages, other._stages);
      _num_of_loaded_shaders = other._num_of_loaded_shaders.load();
    }

    Shader & operator=(Shader &&other) noexcept
    {
      std::swap(_path, other._path);
      std::swap(_modules, other._modules);
      std::swap(_stages, other._stages);
      _num_of_loaded_shaders = other._num_of_loaded_shaders.load();
      return *this;
    }

    // reload shader
    void reload();

  private:
    // compile sources
    void compile();

    // load sources
    std::vector<char> load(uint shd); // TODO : byte type on linux

  public:
    std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> & get_stages() { return _stages; }
    uint get_stages_number() { return _num_of_loaded_shaders; }
  };
} // namespace ter

#endif // __shader_hpp_
