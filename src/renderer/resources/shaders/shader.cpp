#include "resources/shaders/shader.hpp"
#include "utils/path.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "utils/log.hpp"

/**
 * @brief Constructs a Shader object by compiling and loading its shader stages.
 *
 * This constructor initializes a Shader instance by setting up the shader file path relative
 * to the current working directory and a predefined shaders directory. It creates an include string
 * for the shader compiler and processes a mapping of preprocessor definitions into a command-line
 * compatible define string. It then iterates over the available shader stages, compiling each stage
 * if a corresponding source file exists, validating the stage, and creating a unique Vulkan shader
 * module using the device from the application state. The resulting shader modules and their pipeline
 * stage information are stored internally.
 *
 * @param filename Relative filename of the shader source to be compiled.
 * @param define_map Mapping of preprocessor definitions to be applied during shader compilation.
 */
mr::Shader::Shader(const VulkanState &state, std::string_view filename, const std::unordered_map<std::string, std::string> &define_map)
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

      auto [result, module] =
        state.device().createShaderModuleUnique(create_info);
      assert(result == vk::Result::eSuccess);
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

/**
 * @brief Compiles the shader source code for a specific stage into a SPIR-V binary.
 *
 * This function constructs the source and destination file paths based on the shader's base
 * directory and the provided shader stage. It logs the compilation action, checks for the existence
 * of the shader source file, and if present, invokes the glslc compiler with defined preprocessor
 * directives and include paths to compile the shader into a SPIR-V binary.
 *
 * @param stage The shader stage to compile (e.g., Vertex, Fragment).
 *
 * @note The current implementation utilizes the glslc command-line tool and may be replaced with
 *       Google's libshaderc in the future.
 */
void mr::Shader::compile(Shader::Stage stage) const noexcept
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

/**
 * @brief Loads the binary content of a compiled shader for the specified stage.
 *
 * Constructs the full file path by appending the shader stage's name and the ".spv" extension to the current shader path.
 * If the file exists, its contents are read into a vector of characters and returned; otherwise, an empty optional is returned.
 *
 * @param stage The shader stage to load (e.g., vertex, fragment).
 * @return std::optional<std::vector<char>> The binary contents of the shader file if it exists, or std::nullopt otherwise.
 */
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

void mr::Shader::reload() {}
