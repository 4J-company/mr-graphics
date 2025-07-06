#include "render_context.hpp"
#include "model/model.hpp"
#include "renderer.hpp"
#include "resources/buffer/buffer.hpp"
#include "resources/command_unit/command_unit.hpp"
#include "resources/descriptor/descriptor.hpp"
#include "resources/images/image.hpp"
#include "resources/pipelines/graphics_pipeline.hpp"
#include "vkfw/vkfw.hpp"
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

// TODO(dk6): this is temporary changes, while mr::Camera works incorrect
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// TODO(dk6): workaround to pass camera data to material ubo until Scene class will be added
mr::UniformBuffer *s_cam_ubo_ptr;

mr::RenderContext::RenderContext(VulkanGlobalState *state, Window *parent)
  : _parent(parent)
  , _state(state)
  , _extent(parent->extent())
  , _surface(vkfw::createWindowSurfaceUnique(_state.instance(), _parent->window()))
  , _swapchain(_state, _surface.get(), _extent)
  , _depthbuffer(_state, _extent)
  , _image_fence (_state.device().createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}).value)
{
  for (auto _ : std::views::iota(0, gbuffers_number)) {
    _gbuffers.emplace_back(_state, _extent, vk::Format::eR32G32B32A32Sfloat);
  }
  for (int i = 0; i < _swapchain._images.size(); i++) {
    _image_available_semaphore.emplace_back(_state.device().createSemaphoreUnique({}).value);
    _render_finished_semaphore.emplace_back(_state.device().createSemaphoreUnique({}).value);
  }
  _models_render_finished_semaphore = _state.device().createSemaphoreUnique({}).value;
}

mr::RenderContext::~RenderContext() {
  _state.queue().waitIdle();
}

static void update_camera(mr::FPSCamera &cam, mr::UniformBuffer &cam_ubo) noexcept
{
  // cam.cam() = mr::Camera<float>({1}, {-1}, {0, 1, 0});
  // cam.cam().projection() = mr::Camera<float>::Projection(0.25_pi);

  // TODO(dk6): this is temporary changes, while mr::Camera works incorrect
  glm::mat4 view = glm::lookAt(glm::vec3{1.f, 1.f, 1.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  glm::mat4 proj = glm::perspective(glm::radians(45.f), 1920.f / 1080.f, 0.1f, 100.f);
  glm::mat4 vp = proj * view;

  mr::Matr4f mrvp {
    vp[0][0], vp[0][1], vp[0][2], vp[0][3],
    vp[1][0], vp[1][1], vp[1][2], vp[1][3],
    vp[2][0], vp[2][1], vp[2][2], vp[2][3],
    vp[3][0], vp[3][1], vp[3][2], vp[3][3],
  };

  mr::ShaderCameraData cam_data {
    // .vp = cam.viewproj(),
    .vp = mrvp,
    .campos = cam.cam().position(),
    .fov = static_cast<float>(cam.fov()),
    .gamma = cam.gamma(),
    .speed = cam.speed(),
    .sens = cam.sensetivity(),
  };

  cam_ubo.write(std::span<mr::ShaderCameraData> {&cam_data, 1});
}

void mr::RenderContext::render_lights(UniformBuffer &cam_ubo, CommandUnit &command_unit, uint32_t image_index) {
  static DescriptorAllocator descriptor_alloc(_state);

  const std::vector<float> light_vertexes {-1, -1, 1, -1, 1, 1, -1, 1};
  const std::vector light_indexes {0, 1, 2, 2, 3, 0};
  static const VertexBuffer light_vertex_buffer {_state, std::span {light_vertexes}};
  static const IndexBuffer light_index_buffer {_state, std::span {light_indexes}};
  vk::VertexInputAttributeDescription light_descr {
    .location = 0,
    .binding = 0,
    .format = vk::Format::eR32G32Sfloat,
    .offset = 0
  };
  static mr::ShaderHandle light_shader = mr::ResourceManager<Shader>::get().create(mr::unnamed, _state, "light");

  std::array<Shader::ResourceView, gbuffers_number + 1> light_attach;
  for (int i = 0; i < gbuffers_number; i++) {
    light_attach[i] = Shader::ResourceView(0, i, &_gbuffers[i]);
  }
  light_attach.back() = Shader::ResourceView(0, 6, &cam_ubo);

  static DescriptorSet light_set =
    descriptor_alloc.allocate_set(Shader::Stage::Fragment, light_attach).value_or(DescriptorSet());
  static vk::DescriptorSetLayout light_layout = light_set.layout();
  static GraphicsPipeline light_pipeline = GraphicsPipeline(_state,
                                                            *this,
                                                            GraphicsPipeline::Subpass::OpaqueLighting,
                                                            light_shader,
                                                            {&light_descr, 1},
                                                            {&light_layout, 1});

  // --------------
  // above shit
  // --------------

  for (auto &gbuf : _gbuffers) {
    gbuf.switch_layout(_state, vk::ImageLayout::eShaderReadOnlyOptimal);
  }

  vk::RenderingAttachmentInfoKHR swapchain_image_attachment_info {
    .imageView = _swapchain._images[image_index].image_view(),
    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = {vk::ClearColorValue( std::array {0.f, 0.f, 0.f, 0.f})},
  };

  vk::RenderingAttachmentInfoKHR depth_attachment_info {
    .imageView = _depthbuffer.image_view(),
    .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = {vk::ClearDepthStencilValue(1.f, 0)},
  };

  vk::RenderingInfoKHR attachment_info {
    .renderArea = { 0, 0, _extent.width, _extent.height },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &swapchain_image_attachment_info,
  };

  _swapchain._images[image_index].switch_layout(_state, vk::ImageLayout::eColorAttachmentOptimal);
  _depthbuffer.switch_layout(_state, vk::ImageLayout::eDepthStencilAttachmentOptimal);

  command_unit.begin();
  command_unit->beginRendering(&attachment_info);

  vk::Viewport viewport {
    .x = 0, .y = 0,
    .width = static_cast<float>(_extent.width),
    .height = static_cast<float>(_extent.height),
    .minDepth = 0, .maxDepth = 1,
  };
  command_unit->setViewport(0, viewport);

  vk::Rect2D scissors {
    .offset = {0, 0},
    .extent = {
      static_cast<uint32_t>(_extent.width),
      static_cast<uint32_t>(_extent.height),
    },
  };
  command_unit->setScissor(0, scissors);

  command_unit->bindPipeline(vk::PipelineBindPoint::eGraphics,
                             light_pipeline.pipeline());
  command_unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                   light_pipeline.layout(),
                                   0, {light_set.set()}, {});
  command_unit->bindVertexBuffers(0, {light_vertex_buffer.buffer()}, {0});
  command_unit->bindIndexBuffer(light_index_buffer.buffer(), 0, vk::IndexType::eUint32);
  command_unit->drawIndexed(light_indexes.size(), 1, 0, 0, 0);
  _swapchain._images[image_index].switch_layout(_state, vk::ImageLayout::ePresentSrcKHR);

  command_unit->endRendering();
  command_unit.end();
}

void mr::RenderContext::render_models(UniformBuffer &cam_ubo, CommandUnit &command_unit, mr::FPSCamera &cam) {
  static Model model(_state, *this, "ABeautifulGame/ABeautifulGame.gltf");

  // --------------
  // above shit
  // --------------

  update_camera(cam, cam_ubo);

  for (auto &gbuf : _gbuffers) {
    gbuf.switch_layout(_state, vk::ImageLayout::eColorAttachmentOptimal);
  }
  _depthbuffer.switch_layout(_state, vk::ImageLayout::eDepthStencilAttachmentOptimal);

  auto gbufs_attachs = _gbuffers | std::views::transform([](const ColorAttachmentImage &gbuf) {
    return gbuf.attachment_info();
  }) | std::ranges::to<beman::inplace_vector<vk::RenderingAttachmentInfoKHR, gbuffers_number>>();
  auto depth_attachment_info = _depthbuffer.attachment_info();

  vk::RenderingInfoKHR attachment_info {
    .renderArea = { 0, 0, _extent.width, _extent.height },
    .layerCount = 1,
    .colorAttachmentCount = static_cast<uint32_t>(gbufs_attachs.size()),
    .pColorAttachments = gbufs_attachs.data(),
    .pDepthAttachment = &depth_attachment_info,
    // .pStencilAttachment = &depth_attachment_info,
  };

  command_unit.begin();
  command_unit->beginRendering(&attachment_info);

  vk::Viewport viewport {
    .x = 0, .y = 0,
    .width = static_cast<float>(_extent.width),
    .height = static_cast<float>(_extent.height),
    .minDepth = 0, .maxDepth = 1,
  };
  command_unit->setViewport(0, viewport);

  vk::Rect2D scissors {
    .offset = {0, 0},
    .extent = {
      static_cast<uint32_t>(_extent.width),
      static_cast<uint32_t>(_extent.height),
    },
  };
  command_unit->setScissor(0, scissors);

  model.draw(command_unit);

  command_unit->endRendering();
  command_unit.end();
}

void mr::RenderContext::render(mr::FPSCamera &cam)
{
  static UniformBuffer cam_ubo(_state, sizeof(ShaderCameraData));
  static std::once_flag init_cam;
  std::call_once(init_cam, [&]{s_cam_ubo_ptr = &cam_ubo;});

  static CommandUnit command_unit(_state);

  // --------------
  // above shit
  // --------------

  _state.device().waitForFences(_image_fence.get(), VK_TRUE, UINT64_MAX);
  _state.device().resetFences(_image_fence.get());

  static uint32_t image_index = 0;
  uint32_t prev_image_index = image_index;
  _state.device().acquireNextImageKHR(_swapchain._swapchain.get(),
                                      UINT64_MAX,
                                      _image_available_semaphore[image_index].get(),
                                      nullptr,
                                      &image_index);

  render_models(cam_ubo, command_unit, cam);

  vk::PipelineStageFlags models_wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
  std::array models_signal_semaphores = {_models_render_finished_semaphore.get()};

  auto [models_bufs, models_size] = command_unit.submit_info();
  vk::SubmitInfo models_submit_info {
    .pWaitDstStageMask = models_wait_stages,
    .commandBufferCount = models_size,
    .pCommandBuffers = models_bufs,
    .signalSemaphoreCount = models_signal_semaphores.size(),
    .pSignalSemaphores = models_signal_semaphores.data(),
  };

  _state.queue().submit(models_submit_info);

  render_lights(cam_ubo, command_unit, image_index);

  std::array light_wait_semaphores = {
    _image_available_semaphore[prev_image_index].get(),
    _models_render_finished_semaphore.get(),
  };
  vk::PipelineStageFlags light_wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
  std::array light_signal_semaphores = {_render_finished_semaphore[image_index].get()};

  auto [light_bufs, light_size] = command_unit.submit_info();
  vk::SubmitInfo light_submit_info {
    .waitSemaphoreCount = light_wait_semaphores.size(),
    .pWaitSemaphores = light_wait_semaphores.data(),
    .pWaitDstStageMask = light_wait_stages,
    .commandBufferCount = light_size,
    .pCommandBuffers = light_bufs,
    .signalSemaphoreCount = light_signal_semaphores.size(),
    .pSignalSemaphores = light_signal_semaphores.data(),
  };

  _state.queue().submit(light_submit_info, _image_fence.get());

  vk::PresentInfoKHR present_info {
    .waitSemaphoreCount = light_signal_semaphores.size(),
    .pWaitSemaphores = light_signal_semaphores.data(),
    .swapchainCount = 1,
    .pSwapchains = &_swapchain._swapchain.get(),
    .pImageIndices = &image_index,
  };
  _state.queue().presentKHR(present_info);
}
