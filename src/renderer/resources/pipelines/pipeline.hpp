#if !defined(__pipeline_hpp_)
  #define __pipeline_hpp_

  #include "pch.hpp"
  #include "resources/attachment/attachment.hpp"

namespace ter
{
  class Pipeline
  {
  private:
    vk::Pipeline _pipeline;
    vk::PipelineLayout _layout;
    // std::vector?<vk::DescriptorSetLayout> _desctiptor_layouts;
    // std::vector?<Attachment> _attachments;
    // std::vector?<Constant> _constants;

  public:
    Pipeline() = default;
    ~Pipeline();

    void apply();
  };
} // namespace ter

#endif // __pipeline_hpp_
