#if !defined(__graphics_pipeline_hpp_)
  #define __graphics_pipeline_hpp_

  #include "resources/pipelines/pipeline.hpp"

namespace ter
{
  class GraphicsPipeline : Pipeline
  {
  private:
    uint32_t _subpass;

    vk::PrimitiveTopology _topology;
    vk::VertexInputBindingDescription _binding_descriptor;

    uint32_t PipelineParametersFlag;

  public:
    GraphicsPipeline() = default;
    ~GraphicsPipeline() = default;

    GraphicsPipeline(GraphicsPipeline &&other) noexcept = default;
    GraphicsPipeline &operator=(GraphicsPipeline &&other) noexcept = default;

    void recompile();
  };
} // namespace ter
#endif // __graphics_pipeline_hpp_
