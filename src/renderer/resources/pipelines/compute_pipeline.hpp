#ifndef __MR_COMPUTE_PIPELINE_HPP_
#define __MR_COMPUTE_PIPELINE_HPP_

#include "resources/pipelines/pipeline.hpp"

namespace mr {
  class ComputePipeline : public Pipeline {
    private:
      uint32_t _subpass;

      vk::PrimitiveTopology _topology;
      vk::VertexInputBindingDescription _binding_descriptor;

      uint32_t PipelineParametersFlag;
  };
} // namespace mr
#endif // __MR_COMPUTE_PIPELINE_HPP_
