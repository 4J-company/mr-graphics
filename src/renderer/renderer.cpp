#include "renderer.hpp"

// ter::application class defualt constructor (initializes vulkan instance, device ...)
ter::Application::Application()
{
} // End of 'ter::application::application' function

ter::Application::~Application()
{
}

void ter::Application::create_render_resourses()
{
  create_render_pass(_window_contexts[0]->get_swapchain_format());
  _window_contexts[0]->create_framebuffers(*this);

  vk::SemaphoreCreateInfo semaphore_create_info {};
  vk::FenceCreateInfo fence_create_info { .flags = vk::FenceCreateFlagBits::eSignaled };
  vk::Result result;
  std::tie(result, _image_available_semaphore) = _device.createSemaphore(semaphore_create_info);
  assert(result == vk::Result::eSuccess);
  std::tie(result, _render_rinished_semaphore) = _device.createSemaphore(semaphore_create_info);
  assert(result == vk::Result::eSuccess);
  std::tie(result, _fence) = _device.createFence(fence_create_info);
  assert(result == vk::Result::eSuccess);

  _cmd_unit = CommandUnit(*this);

  _shader = Shader(*this, "default");
  _pipeline = GraphicsPipeline(*this, &_shader);
}

void ter::Application::render()
{
  auto result = _device.waitForFences(_fence, VK_TRUE, UINT64_MAX);
  assert(result == vk::Result::eSuccess);
  _device.resetFences(_fence);

  ter::uint image_index = 0;

  result = _device.acquireNextImageKHR(_window_contexts[0]->_swapchain, UINT64_MAX, _image_available_semaphore, nullptr, &image_index);
  assert(result == vk::Result::eSuccess);

  _cmd_unit.begin();

  vk::ClearValue clear_color;
  clear_color.setColor({0, 0, 0, 1});

  vk::RenderPassBeginInfo render_pass_info
  {
    .renderPass = _render_pass,
    .framebuffer = _window_contexts[0]->_framebuffers[image_index].get_framebuffer(),
    .renderArea = {{0, 0}, _window_contexts[0]->_extent},
    .clearValueCount = 1,
    .pClearValues = &clear_color,
  };

  _cmd_unit._cmd_buffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
  _pipeline.apply(_cmd_unit._cmd_buffer);
  _window_contexts[0]->_framebuffers[image_index].set_viewport(_cmd_unit._cmd_buffer);

  _cmd_unit._cmd_buffer.draw(3, 1, 0, 0);

  _cmd_unit.end();

  vk::Semaphore wait_semaphores[] = {_image_available_semaphore};
  vk::PipelineStageFlags wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
  vk::Semaphore signal_semaphores[] = {_render_rinished_semaphore};

  vk::SubmitInfo submit_info
  {
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = wait_semaphores,
    .pWaitDstStageMask = wait_stages,
    .commandBufferCount = 1,
    .pCommandBuffers = &_cmd_unit._cmd_buffer,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = signal_semaphores,
  };

  result = _queue.submit(submit_info, _fence);
  assert(result == vk::Result::eSuccess);

  vk::SwapchainKHR swapchains[] = {_window_contexts[0]->_swapchain};
  vk::PresentInfoKHR present_info
  {
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = signal_semaphores,
    .swapchainCount = 1,
    .pSwapchains = swapchains,
    .pImageIndices = &image_index,
    .pResults = nullptr, // Optional
  };
  result = _queue.presentKHR(present_info);
  assert(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR);
}

std::unique_ptr<ter::Buffer> ter::Application::create_buffer() { return std::make_unique<Buffer>(); }
std::unique_ptr<ter::CommandUnit> ter::Application::create_command_unit() { return std::make_unique<CommandUnit>(); }
