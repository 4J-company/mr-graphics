#include "resources/pipelines/pipeline.hpp"

void mr::Pipeline::apply(vk::CommandBuffer cmd_buffer) const {}

mr::Pipeline::Pipeline(
  const VulkanState &state, Shader *shader,
  const std::vector<std::vector<vk::DescriptorSetLayoutBinding>> &bindings)
    : _shader(shader)
{
  create_layout_sets(state, bindings);
}

void mr::Pipeline::create_layout_sets(
  const VulkanState &state,
  const std::vector<std::vector<vk::DescriptorSetLayoutBinding>> &bindings)
{
  _set_layouts.resize(bindings.size());
  for (int i = 0; i < bindings.size(); i++) {
    if (bindings[i].empty()) {
      continue;
    }
    vk::DescriptorSetLayoutCreateInfo set_layout_create_info {
      .bindingCount = static_cast<uint>(bindings[i].size()),
      .pBindings = bindings[i].data(),
    };
    _set_layouts[i] = state.device()
                        .createDescriptorSetLayoutUnique(set_layout_create_info)
                        .value;
  }

  std::vector<vk::DescriptorSetLayout> desc_layouts(_set_layouts.size());
  for (int i = 0; i < _set_layouts.size(); i++) {
    desc_layouts[i] = _set_layouts[i].get();
  }

  vk::PipelineLayoutCreateInfo pipeline_layout_create_info {
    .setLayoutCount = static_cast<uint>(desc_layouts.size()),
    .pSetLayouts = desc_layouts.data(),
  };
  _layout = state.device()
              .createPipelineLayoutUnique(pipeline_layout_create_info)
              .value;
}