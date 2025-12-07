#include "resources/shaders/shader.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <tbb/parallel_for_each.h>
#include <boost/unordered/unordered_flat_map.hpp>

static uint32_t define_map_hash(const mr::graphics::Shader::DefineMap &define_map) {
  size_t hash = 0;
  for (auto &[key, val] : define_map) {
    boost::hash_combine(hash, boost::hash_value(key));
    boost::hash_combine(hash, boost::hash_value(val));
  }
  return hash;
}

static uint32_t shader_hash_id(const std::filesystem::path &path, const mr::graphics::Shader::DefineMap &define_map) {
  size_t hash = boost::hash_value(path.string());
  boost::hash_combine(hash, define_map);
  return hash;
}

mr::graphics::Shader::Shader(const VulkanState &state, std::filesystem::path path, const DefineMap &define_map)
  : hashid(shader_hash_id(path, define_map))
{
  path = path::shaders_dir / path;

  for (const auto &entry : std::filesystem::recursive_directory_iterator(path)) {
    if (std::filesystem::is_directory(entry)) {
      continue;
    }

    if (entry.path().extension() == ".glsl") {
      from_glsl_directory(state, path, define_map);
      return;
    }
    if (entry.path().extension() == ".slang") {
      from_slang_file(state, path);
      if (!define_map.empty()) {
        MR_WARNING("Compilation of Slang shaders doesn't support defines");
      }
      return;
    }
  }
}

static std::filesystem::path get_glsl_filepath(std::filesystem::path path, vk::ShaderStageFlagBits stage) {
  ASSERT(std::filesystem::is_directory(path), "`get_glsl_filepath` expects directory.\n"
      "It will return path to file in that directory that corresponds to the given stage.");
  auto get_stage_name = [](vk::ShaderStageFlagBits stage) {
    switch (stage) {
      case vk::ShaderStageFlagBits::eVertex: return "vert";
      case vk::ShaderStageFlagBits::eFragment: return "frag";
      case vk::ShaderStageFlagBits::eTessellationControl: return "ctrl";
      case vk::ShaderStageFlagBits::eTessellationEvaluation: return "eval";
      case vk::ShaderStageFlagBits::eGeometry: return "geom";
      case vk::ShaderStageFlagBits::eCompute: return "comp";
      case vk::ShaderStageFlagBits::eTaskEXT: return "task";
      case vk::ShaderStageFlagBits::eMeshEXT: return "mesh";
      default: PANIC("Unsupported shader stage");
    }
  };
  path.append("shader.").concat(get_stage_name(stage)).concat(".glsl");
  return path;
}

static std::optional<std::vector<char>> load_spv_file_from_glsl_path(std::filesystem::path path, vk::ShaderStageFlagBits stage) noexcept
{
  path += ".spv";
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }

  std::fstream stage_file {path, std::fstream::in | std::ios::ate | std::ios::binary};
  int len = stage_file.tellg();
  std::vector<char> source(len);
  stage_file.seekg(0);
  stage_file.read(reinterpret_cast<char *>(source.data()), len);
  stage_file.close();
  return source;
}

bool compile_glsl_file(const std::filesystem::path &src, std::string_view define_string, std::string_view include_string) noexcept
{
  if (!std::filesystem::exists(src) || std::filesystem::is_directory(src)) {
    return false;
  }

  std::filesystem::path dst = src;
  dst.concat(".spv");

  auto argstr = std::format("glslang --target-env vulkan1.4 {} {} -g -o {} {}",
    define_string, include_string, dst.string(), src.string());

  MR_INFO("Compiling shader {}\n\t with defines {}\n\tCommand: {}", src.string(), define_string, argstr);

  std::system(argstr.c_str());
  return true;
}

std::string construct_define_string(const mr::graphics::Shader::DefineMap &define_map) {
  std::stringstream ss;
  for (auto &[name, value] : define_map) {
    ss << "-D" << name << '=' << value << ' ';
  }
  return ss.str();
}

mr::graphics::Shader & mr::graphics::Shader::from_glsl_directory(
    const VulkanState &state, std::filesystem::path path, const DefineMap &define_map)
{
  ASSERT(std::filesystem::is_directory(path), "Passed file to `from_glsl_directory` method", path.string());

  auto define_string = construct_define_string(define_map);
  auto include_string = std::format(" -I{} -I{} ", path.string(), path::shaders_dir.string());

  _modules.resize(_modules.capacity());
  _stages.resize(_stages.capacity());
  std::atomic_int loaded_stages = 0;

  static tbb::concurrent_unordered_map<std::filesystem::path, std::mutex> directory_mutexes;
  std::scoped_lock lock(directory_mutexes[path]);

  tbb::parallel_for_each(std::array {
      vk::ShaderStageFlagBits::eVertex,
      vk::ShaderStageFlagBits::eFragment,
      vk::ShaderStageFlagBits::eTessellationControl,
      vk::ShaderStageFlagBits::eTessellationEvaluation,
      vk::ShaderStageFlagBits::eGeometry,
      vk::ShaderStageFlagBits::eCompute,
      vk::ShaderStageFlagBits::eTaskEXT,
      vk::ShaderStageFlagBits::eMeshEXT,
    },
    [&] (vk::ShaderStageFlagBits stage) {
      auto filepath = get_glsl_filepath(path, stage);
      auto success = compile_glsl_file(filepath, define_string, include_string);
      if (!success) {
        return;
      }
      auto source_optional = load_spv_file_from_glsl_path(filepath, stage);
      if (!source_optional) {
        MR_INFO("Failed to load {}", filepath.string());
        return;
      }

      auto &source = source_optional.value();
      int index = loaded_stages++;

      vk::ShaderModuleCreateInfo create_info {
       .codeSize = source.size(),
       .pCode = reinterpret_cast<const uint *>(source.data())
      };

      auto [result, module] = state.device().createShaderModuleUnique(create_info);
      ASSERT(result == vk::Result::eSuccess, "Failed to create vk::ShaderModule", result);
      _modules[index] = std::move(module);
      _stages[index] = vk::PipelineShaderStageCreateInfo {
        .stage = stage,
        .module = _modules[index].get(),
        .pName = "main",
      };
    }
  );

  if (loaded_stages.load() == 0) {
    MR_INFO("FAILED TO LOAD GLSL DIRECTORY {}", path.string());
  }
  _modules.resize(loaded_stages);
  _stages.resize(loaded_stages);

  return *this;
}

mr::graphics::Shader & mr::graphics::Shader::from_slang_file(const mr::VulkanState &state, std::filesystem::path slang_file) {
  std::vector<mr::importer::Shader> stages = *ASSERT_VAL(mr::importer::compile(slang_file));

  _stages.resize(_stages.capacity());
  _modules.resize(_modules.capacity());
  std::atomic_int loaded_stages = 0;

  tbb::parallel_for_each(stages, [&, this](const auto &stage) {
      vk::ShaderModuleCreateInfo create_info {
       .codeSize = stage.spirv.size(),
       .pCode = reinterpret_cast<const uint *>(stage.spirv.get())
      };

      int index = loaded_stages++;

      auto [result, module] = state.device().createShaderModuleUnique(create_info);
      ASSERT(result == vk::Result::eSuccess, "Failed to create vk::ShaderModule", result);
      _modules[index] = std::move(module);
      _stages[index] = vk::PipelineShaderStageCreateInfo {
        .stage = stage.stage,
        .module = _modules[index].get(),
        .pName = "main",
      };
    }
  );

  _stages.resize(loaded_stages);
  _modules.resize(loaded_stages);

  return *this;
}
