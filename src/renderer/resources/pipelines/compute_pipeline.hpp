#ifndef __MR_COMPUTE_PIPELINE_HPP_
#define __MR_COMPUTE_PIPELINE_HPP_

#include "resources/pipelines/pipeline.hpp"

namespace mr {
inline namespace graphics {
  class ComputePipeline : public Pipeline {
  private:
    constexpr static inline vk::PushConstantRange compute_pipeline_push_constants {
      .stageFlags = vk::ShaderStageFlagBits::eCompute,
      .offset = 0,
      .size = 64,
    };

  public:
    ComputePipeline() = default;

    ComputePipeline(const VulkanState &state,
                    mr::ShaderHandle shader,
                    std::span<const DescriptorSetLayoutHandle> descriptor_layouts);
  };
}
} // namespace mr

#endif // __MR_COMPUTE_PIPELINE_HPP_
