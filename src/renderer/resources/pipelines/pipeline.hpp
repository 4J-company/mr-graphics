#ifndef __MR_PIPELINE_HPP_
#define __MR_PIPELINE_HPP_

#include "resources/descriptor/descriptor.hpp"
#include "resources/shaders/shader.hpp"
#include "vulkan_state.hpp"

namespace mr {
inline namespace graphics {
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
               std::span<const DescriptorSetLayoutHandle> descriptor_layouts,
               std::optional<vk::PushConstantRange> push_constant_range = std::nullopt);

      const vk::Pipeline pipeline() const { return _pipeline.get(); }

      const vk::PipelineLayout layout() const { return _layout.get(); }

      virtual void apply(vk::CommandBuffer cmd_buffer) const;
  };
}
} // namespace mr

#endif // __MR_PIPELINE_HPP_
