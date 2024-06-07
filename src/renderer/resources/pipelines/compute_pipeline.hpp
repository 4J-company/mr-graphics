#if !defined(__compute_pipeline_hpp_)
#define __compute_pipeline_hpp_

#include "resources/pipelines/pipeline.hpp"

namespace mr {
  class ComputePipeline : public Pipeline {
    private:
      uint32_t _subpass;

      vk::PrimitiveTopology _topology;
      vk::VertexInputBindingDescription _binding_descriptor;

      uint32_t PipelineParametersFlag;

    public:
      ComputePipeline() = default;
      ~ComputePipeline() = default;

      ComputePipeline(ComputePipeline &&other) noexcept = default;
      ComputePipeline &operator=(ComputePipeline &&other) noexcept = default;
  };
} // namespace mr
#endif // __compute_pipeline_hpp_
