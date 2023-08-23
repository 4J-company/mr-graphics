#include "resources/pipelines/pipeline.hpp"

mr::Pipeline::~Pipeline()
{
  // if (_layout != VK_NULL_HANDLE)
  //   (void)0;
}

void mr::Pipeline::apply(vk::CommandBuffer cmd_buffer) const {}
