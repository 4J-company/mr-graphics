#include "resources/pipelines/pipeline.hpp"

ter::Pipeline::~Pipeline()
{
  // if (_layout != VK_NULL_HANDLE)
  //   (void)0;
}

void ter::Pipeline::apply(vk::CommandBuffer cmd_buffer) const
{
}
