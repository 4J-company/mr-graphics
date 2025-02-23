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

      mr::ShaderHandle _shader;

    public:
      /**
 * @brief Default constructor for the Pipeline class.
 *
 * Constructs an uninitialized Pipeline object. For proper initialization of Vulkan resources,
 * use the parameterized constructor.
 */
Pipeline() = default;

      Pipeline(const VulkanState &state, mr::ShaderHandle _shader,
               std::span<const vk::DescriptorSetLayout> bindings);

      /**
 * @brief Retrieves the Vulkan pipeline handle.
 *
 * This method returns the Vulkan pipeline handle managed by the Pipeline instance,
 * allowing access to the underlying Vulkan pipeline resource.
 *
 * @return The Vulkan pipeline handle.
 */
const vk::Pipeline pipeline() const { return _pipeline.get(); }

      const vk::PipelineLayout layout() const { return _layout.get(); }

      virtual void apply(vk::CommandBuffer cmd_buffer) const;
  };
} // namespace mr

#endif // __MR_PIPELINE_HPP_
