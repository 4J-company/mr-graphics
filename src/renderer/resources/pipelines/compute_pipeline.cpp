module;
#include "pch.hpp"
export module ComputePipeline;

import Pipeline;

export namespace mr {
  class ComputePipeline : public Pipeline {
    private:
      uint32_t _subpass;

      vk::PrimitiveTopology _topology;
      vk::VertexInputBindingDescription _binding_descriptor;

      uint32_t PipelineParametersFlag;
  };
} // namespace mr
