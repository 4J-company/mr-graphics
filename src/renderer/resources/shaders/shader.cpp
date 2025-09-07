#include "resources/shaders/shader.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

mr::graphics::Shader::Shader(const VulkanState &state, std::string_view filename, const std::unordered_map<std::string, std::string> &define_map)
    : _path(std::filesystem::current_path())
{
  _path /= path::shaders_dir;
  _include_string = std::string(" -I ") + _path.string() + " ";
  _path /= filename;

  std::stringstream ss;
  for (auto &[name, value] : define_map) {
    ss << "-D" << name << '=' << value << ' ';
  }
  _define_string = ss.str();

  std::array<int, max_shader_modules> shd_types;
  std::iota(shd_types.begin(), shd_types.end(), 0);

  std::for_each(
    std::execution::seq, shd_types.begin(), shd_types.end(), [&](int shd_ind) {
      auto stage = static_cast<Stage>(shd_ind);
      compile(stage);
      std::optional<std::vector<char>> source = load(stage);
      ASSERT(_validate_stage(stage, source.has_value()));
      if (!source) {
        // TODO: check if this stage is optional
        return;
      }

      int ind = _num_of_loaded_shaders++;

      vk::ShaderModuleCreateInfo create_info {
       .codeSize = source->size(),
       .pCode = reinterpret_cast<const uint *>(source->data())
      };

      auto [result, module] =
        state.device().createShaderModuleUnique(create_info);
      ASSERT(result == vk::Result::eSuccess);
      _modules[ind] = std::move(module);

      vk::SpecializationInfo *specialization_info_ptr = nullptr;
      vk::SpecializationInfo specialization_info;
      #if 0
        specialization_info_ptr = &specialization_info;
        specialization_info = vk::SpecializationInfo {
          .dataSize = specialization_data.size(),
          .pData = specialization_data.data(),
        };
      #endif

      _stages[ind] = vk::PipelineShaderStageCreateInfo {
        .stage = get_stage_flags(stage),
        .module = _modules[ind].get(),
        .pName = "main",
        .pSpecializationInfo = specialization_info_ptr,
      };
    });
}

// TODO: replace with Google's libshaderc
void mr::graphics::Shader::compile(Shader::Stage stage) const noexcept
{
  MR_INFO("Compiling shader {}\n\t with defines {}\n", _path.string(), _define_string);

  std::string stage_type = get_stage_name(stage);

  std::filesystem::path src_path = _path;
  src_path.append("shader").replace_extension(stage_type);

  if (!std::filesystem::exists(src_path)) return;

  std::filesystem::path dst_path = _path;
  dst_path.append(stage_type).replace_extension("spv");

  auto argstr = ("glslc " + _define_string + _include_string + " -O " +
                 src_path.string() + " -o " + dst_path.string());
  std::system(argstr.c_str());
}

std::optional<std::vector<char>> mr::graphics::Shader::load(Shader::Stage stage) noexcept
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

bool mr::graphics::Shader::_validate_stage(Stage stage, bool present) const noexcept
{
  // TODO: add mesh & task stages
  if (not present && (stage == Stage::Vertex || stage == Stage::Fragment)) {
    return false;
  }

  auto find_stage = [this](Stage stage) {
    return std::ranges::find(_stages,
                             get_stage_flags(stage),
                             &vk::PipelineShaderStageCreateInfo::stage) !=
           _stages.end();
  };

  if (present && stage == Stage::Evaluate) {
    return find_stage(Stage::Control);
  }

  if (present && stage != Stage::Compute) {
    return not find_stage(Stage::Compute);
  }

  return true;
}

void mr::graphics::Shader::reload() {}
