module;
#include "pch.hpp"
export module Pipeline;

import Shader;
import VulkanApplication;

export namespace mr {
  class Pipeline {
    protected:
      vk::UniquePipeline _pipeline;
      vk::UniquePipelineLayout _layout;
      std::vector<vk::UniqueDescriptorSetLayout> _set_layouts;
      // std::vector?<Attachment> _attachments;
      // std::vector?<Constant> _constants;

      Shader *_shader;

    public:
      Pipeline() = default;

      Pipeline(
        const VulkanState &state, Shader *shader,
        const std::vector<std::vector<vk::DescriptorSetLayoutBinding>> &bindings);

      const vk::Pipeline pipeline() const { return _pipeline.get(); }

      const vk::PipelineLayout layout() const { return _layout.get(); }

      virtual void apply(vk::CommandBuffer cmd_buffer) const;

      void create_layout_sets(
        const VulkanState &state,
        const std::vector<std::vector<vk::DescriptorSetLayoutBinding>> &bindings);

      const vk::DescriptorSetLayout set_layout(uint set_number)
      {
        assert(set_number < _set_layouts.size());
        return _set_layouts[set_number].get();
      }
  };
} // namespace mr

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
