#include "resources/shaders/shader.hpp"

ter::Shader::Shader(VulkanApplication &va, std::string_view filename) : _path(filename)
{
  static const vk::ShaderStageFlagBits stage_bits[] =
  {
    vk::ShaderStageFlagBits::eVertex,
    vk::ShaderStageFlagBits::eFragment,
    vk::ShaderStageFlagBits::eGeometry,
    vk::ShaderStageFlagBits::eTessellationControl,
    vk::ShaderStageFlagBits::eTessellationEvaluation,
    vk::ShaderStageFlagBits::eCompute
  };

  std::array<int, max_shader_modules> shd_types;
  std::iota(shd_types.begin(), shd_types.end(), 0);

  for_each(std::execution::seq, shd_types.begin(), shd_types.end(), 
    [&]( INT shd )
    {
      auto sourse = load(shd);
      if (sourse.size() == 0)
        return;

      _num_of_loaded_shaders++;

      vk::ShaderModuleCreateInfo create_info
      {  
        .codeSize = sourse.size(),
        .pCode = reinterpret_cast<const uint *>(sourse.data())
      };

      vk::Result result;
      std::tie(result, _modules[shd]) = va.get_device().createShaderModule(create_info);
      assert(result == vk::Result::eSuccess);

      _stages[shd].stage = stage_bits[shd];
      _stages[shd].module = _modules[shd];
      _stages[shd].pName = "main";
    });
}

/*
ter::Shader::~Shader()
{
  /// TODO: destroy shader modules
}
*/

void ter::Shader::compile() {}

std::vector<byte> ter::Shader::load(uint shd)
{
  std::string project_path = "Z:/megarender2/"; /// TODO : project path

  static const auto is_file_present = 
    []( const std::string &FileName )
    {
      std::ifstream test(FileName);
      return test.good();
    };
  static const auto execute_cmd = 
    [&]( const std::string &CommandString ) 
    {
      std::system(CommandString.c_str());
    };
  static const auto compile_shd = 
    [&]( const std::string &ShaderDir, const std::string &ShaderType ) 
    {
      auto file_glsl = project_path + ShaderDir + "shader." + ShaderType;
      auto file_spv = project_path + ShaderDir + ShaderType + ".spv";

      // Get files data
      struct stat stat_glsl, stat_spv, stat_includes;
      stat(file_glsl.c_str(), &stat_glsl);
      stat(file_spv.c_str(), &stat_spv);
      stat((project_path + "bin/shaders/includes").c_str(), &stat_includes);

      // Compile shader if it was updated
      if (is_file_present(file_glsl) && ((stat_glsl.st_mtime > stat_spv.st_mtime) || (stat_includes.st_mtime > stat_spv.st_mtime))) // compute shader present & updated
        execute_cmd("X:\\VulkanSDK\\Bin\\glslc -Ibin/shaders/includes " + file_glsl + " -o " + file_spv + "\n");
    };
  std::string shader_dir = "bin/shaders/" + _path + '/';

  static const CHAR *shader_type_names[] = { "vert", "frag", "geom", "tesc", "tese", "comp" };
  std::string file_type = shader_type_names[shd];

  /// compile_shd(shader_dir, file_type); TODO : compile  

  std::fstream shader_file(shader_dir + file_type + ".spv", std::fstream::in | std::ios::ate | std::ios::binary);

  if (!shader_file)
    return {};

  /// if (file_type == "tesc" || file_type == "tese")
  ///   IsTessStageActive = TRUE;

  INT len = (INT)shader_file.tellg();
  std::vector<byte> sourse(len);
  shader_file.seekg(0);
  shader_file.read(reinterpret_cast<char *>(sourse.data()), len);
  shader_file.close();

  return sourse;
}

void ter::Shader::reload() {}
