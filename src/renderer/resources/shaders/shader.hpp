#if !defined(__shader_hpp_)
  #define __shader_hpp_

  #include "pch.hpp"

namespace ter
{
  class Shader
  {
    inline static const size_t max_shader_modules = 6;

    std::string _path;
    std::array<vk::ShaderModule, max_shader_modules> _modules;
    std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> _stages;

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
    Shader(std::string_view filename);
    ~Shader();

    // move semantics
    Shader(Shader &&other) noexcept = default;
    Shader &operator=(Shader &&other) noexcept = default;

    // reload shader
    void reload();

  private:
    // compile sources
    void compile();

    // load sources
    void load();
  };
} // namespace ter

#endif // __shader_hpp_
