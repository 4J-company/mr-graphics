#include "resources/shaders/shader.hpp"
#include <filesystem>
#include <fstream>

#include "spirv_reflect.h"
#include "vk_format_utils.h"

mr::Shader::Shader(const VulkanState &state, std::string_view filename)
    : _path(std::string("bin/shaders/") + filename.data())
    , _name(filename)
{
  std::fill(_stage_reloaded.begin(), _stage_reloaded.end(), true);
  _recompiled = true;

  recompile(state);
  reload(state);
}

void mr::Shader::compile(Shader::Stage stage) const noexcept
{
  std::string stage_name = get_stage_name(stage);
  auto shader =
    (_path / (std::string("shader.") + stage_name)).string();
  auto shader_out =
    (_path / (std::string(stage_name) + ".spv")).string();

  std::string command = std::string("glslc ") + shader +
    " -o " + shader_out;
  std::cout << "COMMAND: " << command << '\n';
  std::system(command.c_str());
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

void mr::Shader::reload(const VulkanState &state)
{
  if (not _recompiled) {
    return;
  }
  _recompiled = false;

  std::array<int, max_shader_modules> shd_types;
  std::iota(shd_types.begin(), shd_types.end(), 0);

  _num_of_loaded_shaders = 0;

  _reloaded = true;

  std::for_each(
    std::execution::seq, shd_types.begin(), shd_types.end(), [&](int shd_ind)
    {
      if (not check_stage_exist(shd_ind)) {
        return;
      }
      int ind = _num_of_loaded_shaders++;

      if (not _stage_reloaded[shd_ind]) {
        return;
      }
      _stage_reloaded[shd_ind] = false;

      auto stage = static_cast<Stage>(shd_ind);
      std::optional<std::vector<char>> source = load(stage);
      assert(source);
      assert(_validate_stage(stage, source.has_value()));

      reflect_metadata(source.value(), stage);

      vk::ShaderModuleCreateInfo create_info {
        .codeSize = source->size(),
        .pCode = reinterpret_cast<const uint *>(source->data())
      };

      auto [result, module] =
        state.device().createShaderModuleUnique(create_info);
      assert(result == vk::Result::eSuccess);
      _modules[ind] = std::move(module);

      _stages[ind] = vk::PipelineShaderStageCreateInfo {
        .stage = get_stage_flags(stage),
        .module = _modules[ind].get(),
        .pName = "main",
      };
    });
}

void mr::Shader::recompile(const VulkanState &state)
{
  std::array<int, max_shader_modules> shd_types;
  std::iota(shd_types.begin(), shd_types.end(), 0);

  std::for_each(
    std::execution::seq, shd_types.begin(), shd_types.end(), [&](int shd_ind)
    {
      if (not check_stage_exist(shd_ind)) {
        return;
      }

      auto stage = static_cast<Stage>(shd_ind);
      if (check_need_recompile(shd_ind)) {
        std::cout << "RECOMPILING\n";
        compile(stage);
        _stage_reloaded[shd_ind] = true;
        _recompiled = true;
      }
    });
}

void mr::Shader::hot_recompile_shaders(const VulkanState &state,
                                       std::map<std::string, Shader> &shaders,
                                       std::mutex &mutex) noexcept
{
  std::lock_guard guard(mutex);
  for (auto&& [_, shd] : shaders) {
    shd.recompile(state);
  }
}

void mr::Shader::hot_reload_shaders(const VulkanState &state,
                                    std::map<std::string, Shader> &shaders,
                                    std::mutex &mutex) noexcept
{
  std::lock_guard guard(mutex);
  for (auto&& [_, shd] : shaders) {
    shd.reload(state);
  }
}


bool mr::Shader::check_stage_exist(int stage) {
  std::filesystem::path stage_src_path =
    _path / (std::string("shader.") + get_stage_name(stage));
  return std::filesystem::exists(stage_src_path);
}

bool mr::Shader::check_need_recompile(int stage) {
  std::filesystem::path stage_spv_path =
    _path / (std::string(get_stage_name(stage)) + ".spv");

  std::error_code ec;
  if (!std::filesystem::exists(stage_spv_path, ec)) {
    return true;
  }
  assert(not ec);

  std::filesystem::path stage_src_path =
    _path / (std::string("shader.") + get_stage_name(stage));

  auto src_time = std::filesystem::last_write_time(stage_src_path, ec);
  auto spv_time = std::filesystem::last_write_time(stage_spv_path, ec);

  auto time1 = std::format("File write time {} is {}\n", stage_src_path.string(), src_time);
  auto time2 = std::format("File write time {} is {}\n", stage_spv_path.string(), spv_time);

  return spv_time < src_time;
}

void mr::Shader::reflect_metadata(std::span<char> spv_data, Stage stage)
{
  // Generate reflection data for a shader
  SpvReflectShaderModule module {};
  SpvReflectResult result = spvReflectCreateShaderModule(spv_data.size(), spv_data.data(), &module);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  uint32_t count = 0;
  result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  std::vector<SpvReflectDescriptorSet *> sets(count);
  result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  for (auto *set : sets) {
    _reflected_sets.resize(std::max(_reflected_sets.size(), (size_t)set->set + 1));
    auto &refl_set = _reflected_sets[set->set];

    for (int i = 0; i < set->binding_count; i++) {
      auto *binding = set->bindings[i];
      refl_set.resize(std::max(refl_set.size(), (size_t)binding->binding + 1));
      assert(not refl_set[binding->binding].has_value()); // not overriding by other module
      auto &refl_bind = refl_set[binding->binding].emplace();

      refl_bind.binding = binding->binding;
      refl_bind.type = static_cast<vk::DescriptorType>(binding->descriptor_type);
      refl_bind.set = set->set;
      for (uint32_t i = 0; i < binding->array.dims_count; ++i) {
        refl_bind.descriptor_count *= binding->array.dims[i];
      }
      refl_bind.stages = static_cast<vk::ShaderStageFlagBits>(module.shader_stage);
    }
  }

  if (stage == Stage::Vertex) {
    uint32_t count = 0;
    result = spvReflectEnumerateInputVariables(&module, &count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectInterfaceVariable *> input_variables(count);
    result = spvReflectEnumerateInputVariables(&module, &count, input_variables.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    _reflected_attributes.reserve(count);
    uint32_t offset = 0;
    for (auto *input_var : input_variables) {
      _reflected_attributes.push_back({
        .location = input_var->location,
        .binding = 0, // TODO: what is this mode?
        .format = vk::Format(input_var->format),
        .offset = offset,
       });
      offset += FormatSize(static_cast<VkFormat>(input_var->format));
    }
  }

  spvReflectDestroyShaderModule(&module);
}