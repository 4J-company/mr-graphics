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
      std::vector<vk::UniqueDescriptorSetLayout> _set_layouts;
      // std::vector?<Attachment> _attachments;
      // std::vector?<Constant> _constants;

      Shader *_shader;

    public:
      Pipeline() = default;

      Pipeline(const VulkanState &state, Shader *shader,
               const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>
                 &bindings);

      const vk::Pipeline pipeline() const { return _pipeline.get(); }

      const vk::PipelineLayout layout() const { return _layout.get(); }

      virtual void apply(vk::CommandBuffer cmd_buffer) const;

      void create_layout_sets(
        const VulkanState &state,
        const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>
          &bindings);

      const vk::DescriptorSetLayout set_layout(uint set_number)
      {
        assert(set_number < _set_layouts.size());
        return _set_layouts[set_number].get();
      }
  };
} // namespace mr

#endif // __pipeline_hpp_
