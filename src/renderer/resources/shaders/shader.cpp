#include "resources/shaders/shader.hpp"
#include <filesystem>
#include <fstream>

mr::Shader::Shader(VulkanState state, std::string_view filename) : _path(filename)
{
  static const vk::ShaderStageFlagBits stage_bits[] = {vk::ShaderStageFlagBits::eVertex,
                                                       vk::ShaderStageFlagBits::eFragment,
                                                       vk::ShaderStageFlagBits::eGeometry,
                                                       vk::ShaderStageFlagBits::eTessellationControl,
                                                       vk::ShaderStageFlagBits::eTessellationEvaluation,
                                                       vk::ShaderStageFlagBits::eCompute};

  std::array<int, max_shader_modules> shd_types;
  std::iota(shd_types.begin(), shd_types.end(), 0);

  std::for_each(std::execution::seq, shd_types.begin(), shd_types.end(), [&](int shd) {
    std::vector<char> source = load((ShaderStages)shd);
    if (source.size() == 0)
      return;

    _num_of_loaded_shaders++;

    vk::ShaderModuleCreateInfo create_info {.codeSize = source.size(),
                                            .pCode = reinterpret_cast<const uint *>(source.data())};

    // vk::Result result;
    // std::tie(result, _modules[shd]) = state.device().createShaderModule(create_info);
    _modules[shd] = state.device().createShaderModule(create_info).value;

    _stages[shd].stage = stage_bits[shd];
    _stages[shd].module = _modules[shd];
    _stages[shd].pName = "main";
  });
}

/*
mr::Shader::~Shader()
{
  /// TODO: destroy shader modules
}
*/

void mr::Shader::compile(ShaderStages stage)
{
  static const char *shader_type_names[] = {"vert", "frag", "geom", "tesc", "tese", "comp"};
  std::string stage_type = shader_type_names[(int)stage];
  std::system(("glslc *." + stage_type + " -o " + stage_type + ".spv").c_str());
}

std::vector<char> mr::Shader::load(ShaderStages stage)
{
  static const char *shader_type_names[] = {"vert", "frag", "geom", "tesc", "tese", "comp"};
  std::filesystem::path stage_file_path = _path;

  // Stage path: PROJECT_PATH/bin/shaders/SHADER_NAME/SHADER_TYPE.spv
  stage_file_path.append(shader_type_names[(int)stage]).append(".spv");
  if (!std::filesystem::exists(stage_file_path))
    return {};

  std::fstream stage_file {stage_file_path, std::fstream::in | std::ios::ate | std::ios::binary};
  int len = stage_file.tellg();
  std::vector<char> source(len);
  stage_file.seekg(0);
  stage_file.read(reinterpret_cast<char *>(source.data()), len);
  stage_file.close();
  return source;
}

void mr::Shader::reload() {}
