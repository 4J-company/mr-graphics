#if !defined(__pipeline_hpp_)
#define __pipeline_hpp_

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

      mr::ShaderHandle _shader;

    public:
      Pipeline() = default;

      Pipeline(const VulkanState &state, mr::ShaderHandle _shader,
               std::span<const vk::DescriptorSetLayout> bindings);

      const vk::Pipeline pipeline() const { return _pipeline.get(); }

      const vk::PipelineLayout layout() const { return _layout.get(); }

      virtual void apply(vk::CommandBuffer cmd_buffer) const;
  };
} // namespace mr

#endif // __pipeline_hpp_
