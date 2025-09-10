#include "render_context.hpp"
#include "model/model.hpp"
#include "lights/lights.hpp"
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

mr::RenderContext::RenderContext(VulkanGlobalState *global_state, Window *parent)
  : _parent(parent)
  , _state(std::make_shared<VulkanState>(global_state))
  , _extent(parent->extent())
  , _surface(vkfw::createWindowSurfaceUnique(_state->instance(), _parent->window()))
  , _swapchain(*_state, _surface.get(), _extent)
  , _depthbuffer(*_state, _extent)
  , _image_fence (_state->device().createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}).value)
{
  for (auto _ : std::views::iota(0, gbuffers_number)) {
    _gbuffers.emplace_back(*_state, _extent, vk::Format::eR32G32B32A32Sfloat);
  }
  for (int i = 0; i < _swapchain._images.size(); i++) {
    _image_available_semaphore.emplace_back(_state->device().createSemaphoreUnique({}).value);
    _render_finished_semaphore.emplace_back(_state->device().createSemaphoreUnique({}).value);
  }
  _models_render_finished_semaphore = _state->device().createSemaphoreUnique({}).value;

  // TODO(dk6): real shit
  static UniformBuffer cam_ubo(*_state, sizeof(ShaderCameraData));
  static std::once_flag init_cam;
  std::call_once(init_cam, [&]{s_cam_ubo_ptr = &cam_ubo;});

  _init_lights_render_data();
}

// TODO(dk6): maybe create lights_render_data.cpp file and move this function?
void mr::RenderContext::_init_lights_render_data() {
  const std::vector<float> light_vertexes {-1, -1, 1, -1, 1, 1, -1, 1};
  const std::vector light_indexes {0, 1, 2, 2, 3, 0};

  _lights_render_data.screen_vbuf = VertexBuffer(*_state, std::span {light_vertexes});
  _lights_render_data.screen_ibuf = IndexBuffer(*_state, std::span {light_indexes});

  vk::VertexInputAttributeDescription light_descr {
    .location = 0,
    .binding = 0,
    .format = vk::Format::eR32G32Sfloat,
    .offset = 0
  };

  // Set 0 is shared for all lights type
  std::array<Shader::ResourceView, gbuffers_number + 1> shader_resources;
  for (int i = 0; i < gbuffers_number; i++) {
    shader_resources[i] = Shader::ResourceView(0, i, &_gbuffers[i]);
  }
  // TODO(dk6): correct camera passing (from scene i think)
  shader_resources.back() = Shader::ResourceView(0, 6, s_cam_ubo_ptr);

  _lights_render_data.set0_layout = ResourceManager<DescriptorSetLayout>::get().create(mr::unnamed,
    *_state, vk::ShaderStageFlagBits::eFragment, shader_resources);
  _lights_render_data.set0_descriptor_allocator = DescriptorAllocator(*_state);

  auto set0_optional = _lights_render_data.set0_descriptor_allocator.value()
    .allocate_set(_lights_render_data.set0_layout);
  ASSERT(set0_optional.has_value());
  _lights_render_data.set0_set = std::move(set0_optional.value());

  _lights_render_data.set0_set.update(*_state, shader_resources);

  auto light_type_data = std::views::zip(LightsRenderData::shader_names,
                                         LightsRenderData::shader_resources_descriptions);
  for (const auto &[shader_name, light_shader_resources] : light_type_data) {
    std::string shader_name_str = {shader_name.begin(), shader_name.end()};
    auto shader = ResourceManager<Shader>::get().create(shader_name_str, *_state, shader_name);
    _lights_render_data.shaders.emplace_back(shader);

    auto layout_handle = ResourceManager<DescriptorSetLayout>::get().create(mr::unnamed,
      *_state, vk::ShaderStageFlagBits::eFragment, light_shader_resources);
    _lights_render_data.set1_layouts.emplace_back(layout_handle);

    std::array set_layouts { _lights_render_data.set0_layout, layout_handle };

    // TODO(dk6): here move instead inplace contruct, because without move this doesn't compile
    _lights_render_data.pipelines.emplace_back(GraphicsPipeline(*_state, *this,
      GraphicsPipeline::Subpass::OpaqueLighting, shader,
      {&light_descr, 1}, set_layouts));

    _lights_render_data.set1_descriptor_allocators.emplace_back(*_state);
  }
}

mr::RenderContext::~RenderContext() {
  if (_state) {
    _state->queue().waitIdle();
  }
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
  static std::array directional_lights {
    DirectionalLight {*_state, _lights_render_data, Norm3f(1, 1, -1)},
    DirectionalLight {*_state, _lights_render_data, Norm3f(-1, 1, 1), Vec3f(0.3, 0.47, 0.8)},
  };

  // --------------
  // above shit
  // --------------

  for (auto &gbuf : _gbuffers) {
    gbuf.switch_layout(*_state, vk::ImageLayout::eShaderReadOnlyOptimal);
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

  _swapchain._images[image_index].switch_layout(*_state, vk::ImageLayout::eColorAttachmentOptimal);
  _depthbuffer.switch_layout(*_state, vk::ImageLayout::eDepthStencilAttachmentOptimal);

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

  // shade all
  command_unit->bindVertexBuffers(0, {_lights_render_data.screen_vbuf.buffer()}, {0});
  command_unit->bindIndexBuffer(_lights_render_data.screen_ibuf.buffer(), 0, vk::IndexType::eUint32);

  for (auto &light : directional_lights) {
    light.shade(command_unit);
  }

  _swapchain._images[image_index].switch_layout(*_state, vk::ImageLayout::ePresentSrcKHR);

  command_unit->endRendering();
  command_unit.end();
}

void mr::RenderContext::render_models(UniformBuffer &cam_ubo, CommandUnit &command_unit, mr::FPSCamera &cam) {
  static Model model(*_state, *this, "ABeautifulGame/ABeautifulGame.gltf");

  // --------------
  // above shit
  // --------------

  update_camera(cam, cam_ubo);

  for (auto &gbuf : _gbuffers) {
    gbuf.switch_layout(*_state, vk::ImageLayout::eColorAttachmentOptimal);
  }
  _depthbuffer.switch_layout(*_state, vk::ImageLayout::eDepthStencilAttachmentOptimal);

  auto gbufs_attachs = _gbuffers | std::views::transform([](const ColorAttachmentImage &gbuf) {
    return gbuf.attachment_info();
  }) | std::ranges::to<InplaceVector<vk::RenderingAttachmentInfoKHR, gbuffers_number>>();
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
  static CommandUnit command_unit(*_state);

  UniformBuffer &cam_ubo = *s_cam_ubo_ptr;

  // --------------
  // above shit
  // --------------

  _state->device().waitForFences(_image_fence.get(), VK_TRUE, UINT64_MAX);
  _state->device().resetFences(_image_fence.get());

  static uint32_t image_index = 0;
  uint32_t prev_image_index = image_index;
  _state->device().acquireNextImageKHR(_swapchain._swapchain.get(),
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

  _state->queue().submit(models_submit_info);

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

  _state->queue().submit(light_submit_info, _image_fence.get());

  vk::PresentInfoKHR present_info {
    .waitSemaphoreCount = light_signal_semaphores.size(),
    .pWaitSemaphores = light_signal_semaphores.data(),
    .swapchainCount = 1,
    .pSwapchains = &_swapchain._swapchain.get(),
    .pImageIndices = &image_index,
  };
  _state->queue().presentKHR(present_info);
}
