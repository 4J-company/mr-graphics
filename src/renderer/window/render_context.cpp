#include "render_context.hpp"
#include "model/model.hpp"
#include "lights/lights.hpp"
#include "renderer.hpp"

#include "resources/buffer/buffer.hpp"
#include "resources/descriptor/descriptor.hpp"
#include "resources/images/image.hpp"
#include "resources/pipelines/graphics_pipeline.hpp"
#include "vkfw/vkfw.hpp"
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

mr::RenderContext::RenderContext(VulkanGlobalState *global_state, Extent extent)
  : _state(std::make_shared<VulkanState>(global_state))
  , _command_unit(*_state)
  , _transfer_command_unit(*_state)
  , _extent(extent)
  , _depthbuffer(*_state, _extent)
  , _image_fence (_state->device().createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}).value)
{
  for (auto _ : std::views::iota(0, gbuffers_number)) {
    _gbuffers.emplace_back(*_state, _extent, vk::Format::eR32G32B32A32Sfloat);
  }
  _models_render_finished_semaphore = _state->device().createSemaphoreUnique({}).value;

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
  // just for pipeline layout
  shader_resources.back() = Shader::ResourceView(0, 6, static_cast<UniformBuffer *>(nullptr));

  _lights_render_data.set0_layout = ResourceManager<DescriptorSetLayout>::get().create(mr::unnamed,
    *_state, vk::ShaderStageFlagBits::eFragment, shader_resources);
  _lights_render_data.set0_descriptor_allocator = DescriptorAllocator(*_state);

  auto set0_optional = _lights_render_data.set0_descriptor_allocator.value()
    .allocate_set(_lights_render_data.set0_layout);
  ASSERT(set0_optional.has_value());
  _lights_render_data.set0_set = std::move(set0_optional.value());

  _lights_render_data.set0_set.update(*_state, std::span(shader_resources.data(), gbuffers_number));

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

void mr::RenderContext::_update_camera_buffer(UniformBuffer &uniform_buffer)
{
  std::array camera_buffer_resource {Shader::ResourceView(0, 6, &uniform_buffer)};
  _lights_render_data.set0_set.update(*_state, camera_buffer_resource);
}

mr::RenderContext::~RenderContext() {
  if (_state) {
    _state->queue().waitIdle();
  }
}

mr::WindowHandle mr::RenderContext::create_window() const noexcept
{
  return ResourceManager<Window>::get().create(mr::unnamed, *this, _extent);
}

mr::FileWriterHandle mr::RenderContext::create_file_writer() const noexcept
{
  return ResourceManager<FileWriter>::get().create(mr::unnamed, *this, _extent);
}

mr::SceneHandle mr::RenderContext::create_scene() const noexcept
{
  return ResourceManager<Scene>::get().create(mr::unnamed, *this);
}

void mr::RenderContext::_render_lights(const SceneHandle scene, Presenter &presenter)
{
  for (auto &gbuf : _gbuffers) {
    gbuf.switch_layout(*_state, vk::ImageLayout::eShaderReadOnlyOptimal);
  }

  vk::RenderingAttachmentInfoKHR swapchain_image_attachment_info = presenter.target_image_info();

  vk::RenderingInfoKHR attachment_info {
    .renderArea = { 0, 0, _extent.width, _extent.height },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &swapchain_image_attachment_info,
  };

  _depthbuffer.switch_layout(*_state, vk::ImageLayout::eDepthStencilAttachmentOptimal);

  _command_unit.begin();
  _command_unit->beginRendering(&attachment_info);

  vk::Viewport viewport {
    .x = 0, .y = 0,
    .width = static_cast<float>(_extent.width),
    .height = static_cast<float>(_extent.height),
    .minDepth = 0, .maxDepth = 1,
  };
  _command_unit->setViewport(0, viewport);

  vk::Rect2D scissors {
    .offset = {0, 0},
    .extent = {
      static_cast<uint32_t>(_extent.width),
      static_cast<uint32_t>(_extent.height),
    },
  };
  _command_unit->setScissor(0, scissors);

  // shade all
  _command_unit->bindVertexBuffers(0, {_lights_render_data.screen_vbuf.buffer()}, {0});
  _command_unit->bindIndexBuffer(_lights_render_data.screen_ibuf.buffer(), 0, vk::IndexType::eUint32);

  std::apply([this](const auto &lights) {
    for (auto light_handle : lights) {
      light_handle->shade(_command_unit);
    }
  }, scene->_lights);

  _command_unit->endRendering();
  _command_unit.end();
}

void mr::RenderContext::_render_models(const SceneHandle scene)
{
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

  _command_unit.begin();
  _command_unit->beginRendering(&attachment_info);

  vk::Viewport viewport {
    .x = 0, .y = 0,
    .width = static_cast<float>(_extent.width),
    .height = static_cast<float>(_extent.height),
    .minDepth = 0, .maxDepth = 1,
  };
  _command_unit->setViewport(0, viewport);

  vk::Rect2D scissors {
    .offset = {0, 0},
    .extent = {
      static_cast<uint32_t>(_extent.width),
      static_cast<uint32_t>(_extent.height),
    },
  };
  _command_unit->setScissor(0, scissors);

  for (ModelHandle model : scene->_models) {
    model->draw(_command_unit);
  }

  _command_unit->endRendering();
  _command_unit.end();
}

void mr::RenderContext::render(const SceneHandle scene, Presenter &presenter)
{
  ASSERT(this == &scene->render_context());
  ASSERT(this == &presenter.render_context());

  _state->device().waitForFences(_image_fence.get(), VK_TRUE, UINT64_MAX);
  _state->device().resetFences(_image_fence.get());

  _update_camera_buffer(scene->_camera_uniform_buffer);

  _render_models(scene);

  vk::PipelineStageFlags models_wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
  std::array models_signal_semaphores = {_models_render_finished_semaphore.get()};

  auto [models_bufs, models_size] = _command_unit.submit_info();
  vk::SubmitInfo models_submit_info {
    .pWaitDstStageMask = models_wait_stages,
    .commandBufferCount = models_size,
    .pCommandBuffers = models_bufs,
    .signalSemaphoreCount = models_signal_semaphores.size(),
    .pSignalSemaphores = models_signal_semaphores.data(),
  };

  _state->queue().submit(models_submit_info);

  _render_lights(scene, presenter);

  beman::inplace_vector<vk::Semaphore, 2> light_wait_semaphores = {
    _models_render_finished_semaphore.get(),
  };
  // TODO(dk6): maybe it is temporary workaround
  auto image_available_semaphore = presenter.image_available_semaphore();
  if (image_available_semaphore != nullptr) {
    light_wait_semaphores.emplace_back(image_available_semaphore);
  }

  vk::PipelineStageFlags light_wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
  std::array light_signal_semaphores = {presenter.render_finished_semaphore()};

  auto [light_bufs, light_size] = _command_unit.submit_info();
  vk::SubmitInfo light_submit_info {
    .waitSemaphoreCount = static_cast<uint32_t>(light_wait_semaphores.size()),
    .pWaitSemaphores = light_wait_semaphores.data(),
    .pWaitDstStageMask = light_wait_stages,
    .commandBufferCount = light_size,
    .pCommandBuffers = light_bufs,
    .signalSemaphoreCount = light_signal_semaphores.size(),
    .pSignalSemaphores = light_signal_semaphores.data(),
  };

  _state->queue().submit(light_submit_info, _image_fence.get());

  presenter.present();
}
