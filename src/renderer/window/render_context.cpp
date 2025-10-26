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
  , _models_command_unit(*_state)
  , _lights_command_unit(*_state)
  , _transfer_command_unit(*_state)
  , _extent(extent)
  , _depthbuffer(*_state, _extent)
  , _image_fence (_state->device().createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}).value)
  , _default_descriptor_allocator(*_state)
  , _vertex_buffers_heap(default_vertex_number, 1)
  , _positions_vertex_buffer(*_state, default_vertex_number * position_bytes_size)
  , _attributes_vertex_buffer(*_state, default_vertex_number * attributes_bytes_size)
  , _index_buffer(*_state, default_index_number * sizeof(uint32_t), sizeof(uint32_t))
{
  for (auto _ : std::views::iota(0, gbuffers_number)) {
    _gbuffers.emplace_back(*_state, _extent, vk::Format::eR32G32B32A32Sfloat);
  }
  _models_render_finished_semaphore = _state->device().createSemaphoreUnique({}).value;

  init_bindless_rendering();
  init_lights_render_data();
}

// TODO(dk6): maybe create lights_render_data.cpp file and move this function?
void mr::RenderContext::init_lights_render_data() {
  const std::array light_vertexes {-1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f};
  const std::array light_indexes {0, 1, 2, 2, 3, 0};

  _lights_render_data.screen_vbuf = VertexBuffer(*_state, std::span {light_vertexes});
  _lights_render_data.screen_ibuf = IndexBuffer(*_state, std::span {light_indexes});

  vk::VertexInputAttributeDescription light_descr {
    .location = 0,
    .binding = 0,
    .format = vk::Format::eR32G32Sfloat,
    .offset = 0
  };

  std::array<DescriptorSetLayout::BindingDescription, gbuffers_number> bindings;
  for (uint32_t i = 0; i < gbuffers_number; i++) {
    bindings[i] = {i, vk::DescriptorType::eInputAttachment};
  }

  _lights_render_data.lights_set_layout = ResourceManager<DescriptorSetLayout>::get().create(mr::unnamed,
    *_state, vk::ShaderStageFlagBits::eFragment, bindings);

  auto light_set = _default_descriptor_allocator.allocate_set(_lights_render_data.lights_set_layout);
  ASSERT(light_set.has_value());
  _lights_render_data.lights_descriptor_set = std::move(light_set.value());

  // Set 0 is shared for all lights type
  std::array<Shader::ResourceView, gbuffers_number> shader_resources;
  for (int i = 0; i < gbuffers_number; i++) {
    shader_resources[i] = Shader::ResourceView(i, &_gbuffers[i]);
  }

  _lights_render_data.lights_descriptor_set.update(*_state, std::span(shader_resources.data(), gbuffers_number));

  boost::unordered_map<std::string, std::string> defines {
    {"TEXTURES_BINDING",        std::to_string(textures_binding)},
    {"UNIFORM_BUFFERS_BINDING", std::to_string(uniform_buffer_binding)},
    {"STORAGE_BUFFERS_BINDING", std::to_string(storage_buffer_binding)},
  };

  for (const auto &shader_name : LightsRenderData::shader_names) {
    std::string shader_name_str = {shader_name.begin(), shader_name.end()};
    auto shader = ResourceManager<Shader>::get().create(shader_name_str, *_state, shader_name, defines);
    _lights_render_data.shaders.emplace_back(shader);

    std::array set_layouts {
      _lights_render_data.lights_set_layout,
      DescriptorSetLayoutHandle(_bindless_set_layout),
    };

    // TODO(dk6): here move instead inplace contruct, because without move this doesn't compile
    _lights_render_data.pipelines.emplace_back(GraphicsPipeline(*this,
      GraphicsPipeline::Subpass::OpaqueLighting, shader,
      {&light_descr, 1}, set_layouts));
  }
}

void mr::RenderContext::init_bindless_rendering()
{
  using BindingT = DescriptorSetLayout::BindingDescription;
  std::array bindings {
    BindingT {0, vk::DescriptorType::eCombinedImageSampler},
    BindingT {1, vk::DescriptorType::eUniformBuffer},
    BindingT {2, vk::DescriptorType::eStorageBuffer},
  };

  _bindless_set_layout = ResourceManager<BindlessDescriptorSetLayout>::get().create("BindlessSetLayout",
    *_state, vk::ShaderStageFlagBits::eAllGraphics, bindings);

  auto set = _default_descriptor_allocator.allocate_bindless_set(_bindless_set_layout);
  ASSERT(set.has_value(), "Failed to allocate bindless descriptor set");
  _bindless_set = std::move(set.value());
}

mr::RenderContext::~RenderContext()
{
  ASSERT(_state != nullptr);

  _state->queue().waitIdle();

  _image_available_semaphore.clear();
  _render_finished_semaphore.clear();
  _models_render_finished_semaphore.reset();
  _image_fence.reset();

  _gbuffers.clear();
}

std::vector<VkDeviceSize> mr::RenderContext::add_vertex_buffers(
  const std::vector<std::span<const std::byte>> &vbufs_data) noexcept
{
  // std::println("test heap");
  // GpuHeap heap(4, 1);
  // for (int i = 0; i < 4; i++) {
  //   std::println("test heap i = {}", i);
  //   auto alloc = heap.allocate(1);
  //   ASSERT(alloc.offset == i);
  //   ASSERT(!alloc.resized, "i = {}", i);
  //   std::println("test heap i = {} done", i);
  // }
  // std::println("test heap last");
  // auto alloc = heap.allocate(1);
  // ASSERT(alloc.offset == 4);
  // ASSERT(alloc.resized, "last");
  // std::println("test heap last done");
  // std::println("test heap done");

  // Tmp theme - fixed attributes layout
  ASSERT(vbufs_data.size() == 2);

  auto &positions_data = vbufs_data[0];
  auto &attributes_data = vbufs_data[1];

  ASSERT(positions_data.size() % position_bytes_size == 0);
  ASSERT(attributes_data.size() % attributes_bytes_size == 0);

  ASSERT(positions_data.size() / position_bytes_size == attributes_data.size() / attributes_bytes_size);
  VkDeviceSize vertexes_number = positions_data.size() / position_bytes_size;

  auto alloc_info = _vertex_buffers_heap.allocate(vertexes_number);
  if (alloc_info.resized) {
    _positions_vertex_buffer.resize(_vertex_buffers_heap.size() * position_bytes_size);
    _attributes_vertex_buffer.resize(_vertex_buffers_heap.size() * attributes_bytes_size);
  }

  VkDeviceSize positions_offset = alloc_info.offset * position_bytes_size;
  VkDeviceSize attributes_offset = alloc_info.offset * attributes_bytes_size;

  _positions_vertex_buffer.write(positions_data, positions_offset);
  _attributes_vertex_buffer.write(attributes_data, attributes_offset);

  return std::vector {positions_offset, attributes_offset};
}

void mr::RenderContext::delete_vertex_buffers(const std::vector<VkDeviceSize> &vbufs) noexcept
{
  // Tmp theme - fixed attributes layout
  ASSERT(vbufs.size() == 2);
  uint32_t heap_offset = vbufs[0] / position_bytes_size;
  _vertex_buffers_heap.deallocate(heap_offset);
}

mr::WindowHandle mr::RenderContext::create_window() const noexcept
{
  return create_window(_extent);
}

mr::WindowHandle mr::RenderContext::create_window(const mr::Extent &extent) const noexcept
{
  return ResourceManager<Window>::get().create(mr::unnamed, *this, extent);
}

mr::FileWriterHandle mr::RenderContext::create_file_writer() const noexcept
{
  return ResourceManager<FileWriter>::get().create(mr::unnamed, *this, _extent);
}

mr::SceneHandle mr::RenderContext::create_scene() noexcept
{
  return ResourceManager<Scene>::get().create(mr::unnamed, *this);
}

void mr::RenderContext::render_lights(const SceneHandle scene, Presenter &presenter)
{
  for (auto &gbuf : _gbuffers) {
    gbuf.switch_layout(vk::ImageLayout::eShaderReadOnlyOptimal);
  }

  vk::RenderingAttachmentInfoKHR swapchain_image_attachment_info = presenter.target_image_info();

  vk::RenderingInfoKHR attachment_info {
    .renderArea = { 0, 0, presenter.extent().width, presenter.extent().height },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &swapchain_image_attachment_info,
  };

  _depthbuffer.switch_layout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

  _lights_command_unit->beginRendering(&attachment_info);

  vk::Viewport viewport {
    .x = 0, .y = 0,
    .width = static_cast<float>(presenter.extent().width),
    .height = static_cast<float>(presenter.extent().height),
    .minDepth = 0, .maxDepth = 1,
  };
  _lights_command_unit->setViewport(0, viewport);

  vk::Rect2D scissors {
    .offset = {0, 0},
    .extent = {
      static_cast<uint32_t>(presenter.extent().width),
      static_cast<uint32_t>(presenter.extent().height),
    },
  };
  _lights_command_unit->setScissor(0, scissors);

  // shade all
  _lights_command_unit->bindVertexBuffers(0, {_lights_render_data.screen_vbuf.buffer()}, {0});
  _lights_command_unit->bindIndexBuffer(_lights_render_data.screen_ibuf.buffer(), 0, vk::IndexType::eUint32);

  std::apply([this](const auto &lights) {
    for (auto light_handle : lights) {
      light_handle->shade(_lights_command_unit);
    }
  }, scene->_lights);

  _lights_command_unit->endRendering();
}

void mr::RenderContext::render_models(const SceneHandle scene)
{
  for (auto &gbuf : _gbuffers) {
    gbuf.switch_layout(vk::ImageLayout::eColorAttachmentOptimal);
  }
  _depthbuffer.switch_layout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

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

  _models_command_unit->beginRendering(&attachment_info);

  vk::Viewport viewport {
    .x = 0, .y = 0,
    .width = static_cast<float>(_extent.width),
    .height = static_cast<float>(_extent.height),
    .minDepth = 0, .maxDepth = 1,
  };
  _models_command_unit->setViewport(0, viewport);

  vk::Rect2D scissors {
    .offset = {0, 0},
    .extent = {
      static_cast<uint32_t>(_extent.width),
      static_cast<uint32_t>(_extent.height),
    },
  };
  _models_command_unit->setScissor(0, scissors);

  // ===== Rendering geometry ======

  std::array vertex_buffers {
    _positions_vertex_buffer.buffer(),
    _attributes_vertex_buffer.buffer(),
  };
  std::array vertex_buffers_offsets {0ul, 0ul};
  _models_command_unit->bindVertexBuffers(0, vertex_buffers, vertex_buffers_offsets);

  _models_command_unit->bindIndexBuffer(_index_buffer.buffer(), 0, vk::IndexType::eUint32);

  for (auto &[pipeline, draw] : scene->_draws) {
    _models_command_unit->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->pipeline());

    std::array sets {_bindless_set.set()};
    _models_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                             {pipeline->layout()},
                                             0, // TODO(dk6): give name for this magic number
                                             sets,
                                             {});

    _models_command_unit->pushConstants(pipeline->layout(), vk::ShaderStageFlagBits::eAllGraphics,
                                        0, sizeof(uint32_t), &draw.meshes_render_info_id);

    // TODO(dk6): Update only if scene updates on this frame
    draw.commands_buffer.update();
    draw.meshes_render_info.write(std::span(draw.meshes_render_info_data));

    uint32_t stride = sizeof(vk::DrawIndexedIndirectCommand);
    _models_command_unit->drawIndexedIndirect(draw.commands_buffer.buffer(), 0, draw.meshes.size(), stride);

    // for (auto [draw_number, mesh] : std::views::enumerate(draw.meshes)) {
    //   vk::ConditionalRenderingBeginInfoEXT conditional_rendering_begin_info {
    //     .buffer = scene->_visibility.buffer(),
    //     .offset = mesh->_mesh_offset * sizeof(uint32_t),
    //   };

    //   _state->dispatch_table().cmdBeginConditionalRenderingEXT(
    //     _models_command_unit.command_buffer(),
    //     conditional_rendering_begin_info
    //   );

    //   uint32_t stride = sizeof(vk::DrawIndexedIndirectCommand);
    //   _models_command_unit->drawIndexedIndirect(draw.commands_buffer.buffer(), draw_number, 1, stride);

    //   _state->dispatch_table().cmdEndConditionalRenderingEXT(_models_command_unit.command_buffer());
    // }
  }

  _models_command_unit->endRendering();
}

void mr::RenderContext::resize(const mr::Extent &extent)
{
  _extent = extent;
}

void mr::RenderContext::render(const SceneHandle scene, Presenter &presenter)
{
  ASSERT(this == &scene->render_context());
  ASSERT(this == &presenter.render_context());

  _state->device().waitForFences(_image_fence.get(), VK_TRUE, UINT64_MAX);
  _state->device().resetFences(_image_fence.get());

  resize(presenter.extent());
  // NOTE: Camera UBO is already updated and this resize will only affect next frame
  scene->_camera.cam().projection().resize((float)_extent.width / _extent.height);

  // --------------------------------------------------------------------------
  // Model rendering pass
  // --------------------------------------------------------------------------

  _models_command_unit.begin();
  render_models(scene);

  _models_command_unit.add_signal_semaphore(_models_render_finished_semaphore.get());
  _models_command_unit.end();
  vk::SubmitInfo models_submit_info = _models_command_unit.submit_info();

  _state->queue().submit(models_submit_info);

  // --------------------------------------------------------------------------
  // Lights shading pass
  // --------------------------------------------------------------------------

  _lights_command_unit.begin();
  render_lights(scene, presenter);

  _lights_command_unit.add_wait_semaphore(_models_render_finished_semaphore.get(),
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput);
  auto image_available_semaphore = presenter.image_available_semaphore();
  if (image_available_semaphore) {
    _lights_command_unit.add_wait_semaphore(image_available_semaphore,
                                     vk::PipelineStageFlagBits::eColorAttachmentOutput);
  }
  _lights_command_unit.add_signal_semaphore(presenter.render_finished_semaphore());
  _lights_command_unit.end();

  vk::SubmitInfo light_submit_info = _lights_command_unit.submit_info();

  _state->queue().submit(light_submit_info, _image_fence.get());

  presenter.present();
}
