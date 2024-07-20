#if !defined(__pipeline_hpp_)
#define __pipeline_hpp_

#include "pch.hpp"
#include "resources/attachment/attachment.hpp"
#include "resources/shaders/shader.hpp"
#include "vulkan_application.hpp"

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

      Shader * shader() const { return _shader; }
  };
} // namespace mr

#endif // __pipeline_hpp_
