#if !defined(__pipeline_hpp_)
  #define __pipeline_hpp_

  #include "pch.hpp"
  #include "resources/attachment/attachment.hpp"
  #include "resources/shaders/shader.hpp"
  #include "vulkan_application.hpp"

namespace mr
{
  class Pipeline
  {
  protected:
    vk::Pipeline _pipeline;
    vk::PipelineLayout _layout;
    vk::PipelineCache _pipeline_cache;
    // std::vector?<vk::DescriptorSetLayout> _desctiptor_layouts;
    // std::vector?<Attachment> _attachments;
    // std::vector?<Constant> _constants;

    Shader *_shader;

  public:
    Pipeline() = default;
    ~Pipeline();

    vk::Pipeline pipeline() const { return _pipeline; }
    vk::PipelineLayout layout() const { return _layout; }

    virtual void apply(vk::CommandBuffer cmd_buffer) const;
  };
} // namespace mr

#endif // __pipeline_hpp_
