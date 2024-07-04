#include "resources/shaders/shader.hpp"
#include <filesystem>
#include <fstream>

mr::Shader::Shader(const VulkanState &state, std::string_view filename)
    : _path(std::string("bin/shaders/") + filename.data())
{
  std::array<int, max_shader_modules> shd_types;
  std::iota(shd_types.begin(), shd_types.end(), 0);

  std::for_each(
    std::execution::par, shd_types.begin(), shd_types.end(), [&](int shd_ind) {
      auto stage = static_cast<Stage>(shd_ind);
      std::optional<std::vector<char>> source = load(stage);
      assert(_validate_stage(stage, source.has_value()));
      if (!source) {
        // TODO: check if this stage is optional
        return;
      }

      int ind = _num_of_loaded_shaders++;

      vk::ShaderModuleCreateInfo create_info {
        .codeSize = source->size(),
        .pCode = reinterpret_cast<const uint *>(source->data())
      };

      // TODO: handle this error
      _modules[ind] =
        state.device().createShaderModuleUnique(create_info).value;
      _stages[ind] = vk::PipelineShaderStageCreateInfo{
        .stage = get_stage_flags(stage),
        .module = _modules[ind].get(),
        .pName = "main",
      };
    });
}

void mr::Shader::compile(Shader::Stage stage) const noexcept
{
  std::string stage_type = get_stage_name(stage);
  std::system(("glslc --target-env=vulkan1.2 *." + stage_type + " -o " + stage_type + ".spv").c_str());
}

std::optional<std::vector<char>> mr::Shader::load(Shader::Stage stage) noexcept
{
  std::filesystem::path stage_file_path = _path;

  // Stage path: PROJECT_PATH/bin/shaders/SHADER_NAME/SHADER_TYPE.spv
  stage_file_path.append(get_stage_name(stage));
  stage_file_path += ".spv";
  if (!std::filesystem::exists(stage_file_path)) {
    return std::nullopt;
  }

  std::fstream stage_file {stage_file_path,
                           std::fstream::in | std::ios::ate | std::ios::binary};
  int len = stage_file.tellg();
  std::vector<char> source(len);
  stage_file.seekg(0);
  stage_file.read(reinterpret_cast<char *>(source.data()), len);
  stage_file.close();
  return source;
}

bool mr::Shader::_validate_stage(Stage stage, bool present) const noexcept
{
  // TODO: add mesh & task stages
  if (not present && (stage == Stage::Vertex || stage == Stage::Fragment))
    return false;

  auto find_stage =
    [this](Stage stage) {
      return std::ranges::find(_stages, get_stage_flags(stage), &vk::PipelineShaderStageCreateInfo::stage) != _stages.end();
    };

  if (present && stage == Stage::Evaluate)
    return find_stage(Stage::Control);

  if (present && stage != Stage::Compute)
    return not find_stage(Stage::Compute);

  return true;
}

void mr::Shader::reload() {}
