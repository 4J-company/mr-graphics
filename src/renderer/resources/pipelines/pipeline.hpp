#if !defined(__pipeline_hpp_)
#define __pipeline_hpp_

#include "pch.hpp"
#include "resources/attachment/attachment.hpp"
#include "resources/shaders/shader.hpp"
#include "vulkan_application.hpp"

namespace ter
{
  class Pipeline
  {
  protected:
    vk::Pipeline _pipeline;
    vk::PipelineLayout _layout;
    // std::vector?<vk::DescriptorSetLayout> _desctiptor_layouts;
    // std::vector?<Attachment> _attachments;
    // std::vector?<Constant> _constants;

    Shader *_shader;

  public:
    Pipeline() = default;
    ~Pipeline();

    virtual void apply(vk::CommandBuffer cmd_buffer) const;
  };
} // namespace ter

#endif // __pipeline_hpp_
