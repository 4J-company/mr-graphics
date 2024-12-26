#ifndef __MR_PIPELINE_HPP_
#define __MR_PIPELINE_HPP_

#include "pch.hpp"
#include "resources/attachment/attachment.hpp"
#include "resources/shaders/shader.hpp"
#include "vulkan_state.hpp"

namespace mr {
  class Pipeline {
    protected:
      vk::UniquePipeline _pipeline;
      vk::UniquePipelineLayout _layout;
      // std::vector?<Attachment> _attachments;
      // std::vector?<Constant> _constants;

      Shader *_shader;

    public:
      Pipeline() = default;

      Pipeline(const VulkanState &state, Shader *_shader,
               std::span<const vk::DescriptorSetLayout> bindings);

      const vk::Pipeline pipeline() const { return _pipeline.get(); }

      const vk::PipelineLayout layout() const { return _layout.get(); }

      virtual void apply(vk::CommandBuffer cmd_buffer) const;
  };
} // namespace mr

#endif // __MR_PIPELINE_HPP_
