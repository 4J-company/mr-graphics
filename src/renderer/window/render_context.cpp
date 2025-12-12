#include "render_context.hpp"
#include "model/model.hpp"
#include "lights/lights.hpp"
#include "renderer.hpp"

#include "resources/buffer/buffer.hpp"
#include "resources/descriptor/descriptor.hpp"
#include "resources/images/image.hpp"
#include "resources/pipelines/graphics_pipeline.hpp"
#include "vkfw/vkfw.hpp"
#include "window/dummy_presenter.hpp"
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

#define TRACY_VK_ZONE_BEGIN()

mr::RenderContext::RenderContext(VulkanGlobalState *global_state, Extent extent, RenderOptions options)
  : _state(std::make_shared<VulkanState>(global_state))
  , _render_options(options)
  , _models_command_unit(*_state)
  , _late_models_command_unit(*_state)
  , _lights_command_unit(*_state)
  , _pre_model_layout_transition_semaphore(_state->device().createSemaphoreUnique({}).value)
  , _pre_model_layout_transition_command_unit(*_state)
  , _pre_model_layout_transition_command_unit_late(*_state)
  , _pre_model_layout_transition_semaphore_late(_state->device().createSemaphoreUnique({}).value)
  , _pre_light_layout_transition_semaphore(_state->device().createSemaphoreUnique({}).value)
  , _pre_light_layout_transition_command_unit(*_state)
  , _culling_command_unit(*_state)
  , _late_culling_command_unit(*_state)
  , _culling_semaphore(_state->device().createSemaphoreUnique({}).value)
  , _late_culling_semaphore(_state->device().createSemaphoreUnique({}).value)
  , _visible_models_rendering_semaphore(_state->device().createSemaphoreUnique({}).value)
  , _extent(extent)
  , _depthbuffer(*_state, _extent)
  , _image_fence (_state->device().createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}).value)
  , _default_descriptor_allocator(*_state)
  , _vertex_buffers_heap(default_vertex_number, 1)
  , _positions_vertex_buffer(*_state, default_vertex_number * position_bytes_size)
  , _attributes_vertex_buffer(*_state, default_vertex_number * attributes_bytes_size)
  , _index_buffer(*_state, default_index_number * sizeof(uint32_t), sizeof(uint32_t))
  , _depth_pyramid_extent(extent.width / 2, extent.height / 2)
  , _depth_pyramid(*_state, _depth_pyramid_extent, vk::Format::eR32Sfloat,
                   calculate_mips_levels_number(_depth_pyramid_extent))
{
  for (auto _ : std::views::iota(0, gbuffers_number)) {
    _gbuffers.emplace_back(*_state, _extent, vk::Format::eR32G32B32A32Sfloat);
  }
  _models_render_finished_semaphore = _state->device().createSemaphoreUnique({}).value;

  init_bindless_rendering();
  init_lights_render_data();
  init_profiling();
  init_culling();
  init_bound_box_rendering();
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
      _converted_bindless_set_layout,
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
    BindingT {textures_binding, vk::DescriptorType::eCombinedImageSampler},
    BindingT {uniform_buffer_binding, vk::DescriptorType::eUniformBuffer},
    BindingT {storage_buffer_binding, vk::DescriptorType::eStorageBuffer},
    BindingT {storage_images_binding, vk::DescriptorType::eStorageImage},
  };

  _bindless_set_layout = ResourceManager<BindlessDescriptorSetLayout>::get().create("BindlessSetLayout",
    *_state, vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute, bindings);

  auto set = _default_descriptor_allocator.allocate_bindless_set(_bindless_set_layout);
  ASSERT(set.has_value(), "Failed to allocate bindless descriptor set");
  _bindless_set = std::move(set.value());

  _converted_bindless_set_layout = DescriptorSetLayoutHandle(_bindless_set_layout);
}

void mr::RenderContext::init_profiling()
{
  vk::QueryPoolCreateInfo query_pool_create_info {
    .queryType = vk::QueryType::eTimestamp,
    .queryCount = timestamps_number,
  };
  auto [res, query_pool] = _state->device().createQueryPoolUnique(query_pool_create_info);
  ASSERT(res == vk::Result::eSuccess);
  _timestamps_query_pool = std::move(query_pool);

  uint64_t timestamp_period = _state->phys_device().getProperties().limits.timestampPeriod; // ns per clock
  _timestamp_to_ms = timestamp_period / 1e6;

  _models_tracy_gpu_context = TracyVkContext(_state->phys_device(), _state->device(),
                                             _state->queue(), _models_command_unit.command_buffer());
  char gpu_context_name[] = "models tracy context";
  TracyVkContextName(_models_tracy_gpu_context, gpu_context_name, sizeof(gpu_context_name));

  _lights_tracy_gpu_context = TracyVkContext(_state->phys_device(), _state->device(),
                                             _state->queue(), _lights_command_unit.command_buffer());
  char light_content_name[] = "lights tracy context";
  TracyVkContextName(_lights_tracy_gpu_context, light_content_name, sizeof(light_content_name));
}

void mr::RenderContext::init_culling()
{
  // ---------------------------
  // Frustum culling
  // ---------------------------

  boost::unordered_map<std::string, std::string> defines {
    {"TEXTURES_BINDING",        std::to_string(textures_binding)},
    {"UNIFORM_BUFFERS_BINDING", std::to_string(uniform_buffer_binding)},
    {"STORAGE_BUFFERS_BINDING", std::to_string(storage_buffer_binding)},
    {"STORAGE_IMAGES_BINDING", std::to_string(storage_images_binding)},
    {"BINDLESS_SET", std::to_string(bindless_set_number)},
    {"THREADS_NUM", std::to_string(culling_work_group_size)},
  };
  if (is_render_option_enabled(_render_options, RenderOptions::DisableCulling)) {
    defines.insert({"DISABLE_CULLING", "ON"});
  }

  std::array set_layouts {
    _converted_bindless_set_layout,
  };

  _instances_culling_shader = ResourceManager<Shader>::get().create("InstancesCullingShader",
    *_state, "culling/instances_culling", defines);
  _instances_culling_pipeline = ComputePipeline(*_state, _instances_culling_shader, set_layouts);

  _instances_collect_shader = ResourceManager<Shader>::get().create("InstancesCollectShader",
    *_state, "culling/instances_collect", defines);
  _instances_collect_pipeline = ComputePipeline(*_state, _instances_collect_shader, set_layouts);

  // ---------------------------
  // Depth pyramid
  // ---------------------------

  _depth_pyramid_shader = ResourceManager<Shader>::get().create("DepthPyramidBuild",
    *_state, "culling/depth_pyramid_build", defines);
  _depth_pyramid_pipeline = ComputePipeline(*_state, _depth_pyramid_shader, set_layouts);

  // TODO(dk6): maybe move it in PyramidImage ctr
  CommandUnit cmd(*_state);
  cmd.begin();
  _depth_pyramid.switch_layout(cmd, vk::ImageLayout::eGeneral);
  cmd.end();
  UniqueFenceGuard guard(_state->device(), cmd.submit(*_state));

  _depth_image_attacment_id = _bindless_set.register_resource(&_depthbuffer);
  for (uint32_t mip = 0; mip < _depth_pyramid.mip_levels_number(); mip++) {
    Shader::PyramidImageResource res {
      .image = &_depth_pyramid,
      .mip_level = mip,
    };
    _depth_pyramid_mips.emplace_back(_bindless_set.register_resource(&res));
  }

  _depth_pyramid_sampler = Sampler(*_state, vk::Filter::eLinear, vk::SamplerMipmapMode::eNearest,
                                    vk::SamplerAddressMode::eClampToEdge, vk::SamplerReductionMode::eMax,
                                   _depth_pyramid.mip_levels_number());
  Shader::SamplerStorageImage depth_pyramid_res {
    .storage_image = &_depth_pyramid,
    .sampler = &_depth_pyramid_sampler,
  };
  _depth_pyramid_image_id = _bindless_set.register_resource(&depth_pyramid_res);

  _depth_pyramid_shader_data = StorageBuffer(*_state, std::span((_depth_pyramid_mips)));
  _depth_pyramid_shader_data_buffer_id = _bindless_set.register_resource(&_depth_pyramid_shader_data);

  // ---------------------------
  // Late culling
  // ---------------------------

  defines["MAX_DEPTH_PYRAMID_LEVELS"] = std::to_string(depth_pyramid_max_levels);
  _late_instances_culling_shader = ResourceManager<Shader>::get().create("LateInstancesCullingShader",
    *_state, "culling/late_instances_culling", defines);
  _late_instances_culling_pipeline = ComputePipeline(*_state, _late_instances_culling_shader, set_layouts);
}

void mr::RenderContext::init_bound_box_rendering()
{
  boost::unordered_map<std::string, std::string> defines {
    {"TEXTURES_BINDING",        std::to_string(textures_binding)},
    {"UNIFORM_BUFFERS_BINDING", std::to_string(uniform_buffer_binding)},
    {"STORAGE_BUFFERS_BINDING", std::to_string(storage_buffer_binding)},
    {"BINDLESS_SET", std::to_string(bindless_set_number)},
  };
  _bound_boxes_draw_shader = ResourceManager<Shader>::get().create("BoundBoxShader", *_state, "bound_box", defines);

  std::array set_layouts {
    _converted_bindless_set_layout,
  };
  _bound_boxes_draw_pipeline =
    GraphicsPipeline(*this, GraphicsPipeline::Subpass::OpaqueGeometry, _bound_boxes_draw_shader, {}, set_layouts);

  // TODO(dk6): use dynamic buffer
  _bound_boxes_buffer = StorageBuffer(*_state, sizeof(BoundBoxRenderData) * 10000);
  _bound_boxes_buffer_id = _bindless_set.register_resource(&_bound_boxes_buffer);
}

void mr::RenderContext::draw_bound_box(uint32_t transforms_buffer_id, uint32_t transform_index,
                                       uint32_t bound_boxes_buffer_id, uint32_t bound_box_index) noexcept
{
  _bound_boxes_data.emplace_back(BoundBoxRenderData {
    .transforms_buffer_id = transforms_buffer_id,
    .transform_index = transform_index,
    .bound_boxes_buffer_id = bound_boxes_buffer_id,
    .bound_box_index = bound_box_index,
  });
  _bound_boxes_data_dirty = true;
}

mr::RenderContext::~RenderContext()
{
  ASSERT(_state != nullptr);

  TracyVkDestroy(_models_tracy_gpu_context);
  TracyVkDestroy(_lights_tracy_gpu_context);

  _state->queue().waitIdle();

  _image_available_semaphore.clear();
  _render_finished_semaphore.clear();
  _models_render_finished_semaphore.reset();
  _image_fence.reset();

  _gbuffers.clear();
}

mr::VertexBuffersArray mr::RenderContext::add_vertex_buffers(CommandUnit &command_unit,
  std::span<const std::span<const std::byte>> vbufs_data) noexcept
{
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

  _positions_vertex_buffer.write(command_unit, positions_data, positions_offset);
  _attributes_vertex_buffer.write(command_unit, attributes_data, attributes_offset);

  return VertexBuffersArray {
    VertexBufferDescription {
      .offset = positions_offset,
      .vertex_count = static_cast<uint32_t>(positions_data.size()) / position_bytes_size,
    },
    VertexBufferDescription {
      .offset = attributes_offset,
      .vertex_count = static_cast<uint32_t>(attributes_data.size()) / attributes_bytes_size,
    },
  };
}

void mr::RenderContext::delete_vertex_buffers(std::span<const VertexBufferDescription> vbufs) noexcept
{
  // Tmp theme - fixed attributes layout
  ASSERT(vbufs.size() == 2);
  uint32_t heap_offset = vbufs[0].offset / position_bytes_size;
  _vertex_buffers_heap.deallocate(heap_offset);
}

mr::WindowHandle mr::RenderContext::create_window() const noexcept
{
  return create_window(_extent);
}

mr::WindowHandle mr::RenderContext::create_window(const mr::Extent &extent) const noexcept
{
  return ResourceManager<Window>::get().create(mr::unnamed, *this, extent,
    is_render_option_enabled(_render_options, RenderOptions::EnableVsync));
}

mr::FileWriterHandle mr::RenderContext::create_file_writer() const noexcept
{
  return create_file_writer(_extent);
}

mr::FileWriterHandle mr::RenderContext::create_file_writer(const mr::Extent &extent) const noexcept
{
  return ResourceManager<FileWriter>::get().create(mr::unnamed, *this, extent);
}

mr::DummyPresenterHandle mr::RenderContext::create_dummy_presenter(const mr::Extent &extent) const noexcept
{
  return ResourceManager<DummyPresenter>::get().create(mr::unnamed, *this, extent);
}

mr::SceneHandle mr::RenderContext::create_scene() noexcept
{
  return ResourceManager<Scene>::get().create(mr::unnamed, *this);
}

void mr::RenderContext::render_lights(const SceneHandle scene, Presenter &presenter)
{
  ZoneScoped;
  _lights_command_unit.begin();

  TRACY_VK_ZONE_BEGIN() {
    TracyVkZone(_lights_tracy_gpu_context, _lights_command_unit.command_buffer(), "Light pass GPU");

    _lights_command_unit->resetQueryPool(_timestamps_query_pool.get(), enum_cast(Timestamp::ShadingStart), 2);
    _lights_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe,
                                         _timestamps_query_pool.get(),
                                         enum_cast(Timestamp::ShadingStart));

    // CommandUnit command_unit {scene->render_context().vulkan_state()};
    _pre_light_layout_transition_command_unit.begin();
    for (auto &gbuf : _gbuffers) {
      gbuf.switch_layout(_pre_light_layout_transition_command_unit, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
    _depthbuffer.switch_layout(_pre_light_layout_transition_command_unit, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    _pre_light_layout_transition_command_unit.end();
    _pre_light_layout_transition_command_unit.add_signal_semaphore(
        _pre_light_layout_transition_semaphore.get());
    _lights_command_unit.add_wait_semaphore(
      _pre_light_layout_transition_semaphore.get(),
      vk::PipelineStageFlagBits::eFragmentShader
    );

    vk::RenderingAttachmentInfoKHR swapchain_image_attachment_info = presenter.target_image_info();

    vk::RenderingInfoKHR attachment_info {
      .renderArea = { 0, 0, presenter.extent().width, presenter.extent().height },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &swapchain_image_attachment_info,
    };

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

    _lights_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe,
                                         _timestamps_query_pool.get(),
                                         enum_cast(Timestamp::ShadingEnd));
  }

  TracyVkCollect(_lights_tracy_gpu_context, _lights_command_unit.command_buffer());
  _lights_command_unit.end();
}

void mr::RenderContext::render_models(const SceneHandle scene, CommandUnit &cmd_unit)
{
  cmd_unit->writeTimestamp(vk::PipelineStageFlagBits::eDrawIndirect,
                                       _timestamps_query_pool.get(),
                                       enum_cast(Timestamp::ModelsStart));

  std::array vertex_buffers {
    _positions_vertex_buffer.buffer(),
    _attributes_vertex_buffer.buffer(),
  };
  std::array vertex_buffers_offsets {0ul, 0ul};
  cmd_unit->bindVertexBuffers(0, vertex_buffers, vertex_buffers_offsets);

  cmd_unit->bindIndexBuffer(_index_buffer.buffer(), 0, vk::IndexType::eUint32);

  for (auto &[pipeline, draw] : scene->_draws) {
    cmd_unit->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->pipeline());

    std::array sets {_bindless_set.set()};
    cmd_unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                             {pipeline->layout()},
                                             bindless_set_number,
                                             sets,
                                             {});

    uint32_t model_push_constant[] {
      draw.meshes_render_info_id,
      scene->camera_buffer_id(),
      is_render_option_enabled(_render_options, RenderOptions::DisableCulling)
        ? scene->_transforms_buffer_id : scene->_render_transforms_buffer_id,
    };

    cmd_unit->pushConstants(pipeline->layout(), vk::ShaderStageFlagBits::eAllGraphics,
                                        0, sizeof(model_push_constant), model_push_constant);

    uint32_t stride = sizeof(vk::DrawIndexedIndirectCommand);
    uint32_t max_draws_count = static_cast<uint32_t>(draw.meshes_data_buffer_data.size());
    cmd_unit->drawIndexedIndirectCount(draw.draw_commands_buffer.buffer(), 0,
                                                   scene->_counters_buffer.buffer(),
                                                   draw.draw_counter_index * sizeof(uint32_t),
                                                   max_draws_count, stride);
  }

  cmd_unit->writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe,
                                       _timestamps_query_pool.get(),
                                       enum_cast(Timestamp::ModelsEnd));
}

void mr::RenderContext::culling_geometry(const SceneHandle scene)
{
  _culling_command_unit.begin();

  _culling_command_unit->resetQueryPool(_timestamps_query_pool.get(), enum_cast(Timestamp::CullingStart), 2);

  _culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe,
                                       _timestamps_query_pool.get(),
                                       enum_cast(Timestamp::CullingStart));

  // ===== Fill all counters by zeroes =====
  _culling_command_unit->fillBuffer(scene->_counters_buffer.buffer(), 0, scene->_counters_buffer.byte_size(), 0);
  vk::BufferMemoryBarrier set_count_to_zero_barrier {
    .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
    .dstAccessMask = vk::AccessFlagBits::eShaderRead,
    .buffer = scene->_counters_buffer.buffer(),
    .offset = 0,
    .size = scene->_counters_buffer.byte_size(),
  };
  _culling_command_unit->pipelineBarrier(
    vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
    {}, {set_count_to_zero_barrier}, {});

  std::array culling_descriptor_sets {_bindless_set.set()};

  // ===== Setup and call culling instances shader =====
  if (not is_render_option_enabled(_render_options, RenderOptions::DisableCulling)) {
    _culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _instances_culling_pipeline.pipeline());

    _culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                             {_instances_culling_pipeline.layout()},
                                             bindless_set_number,
                                             culling_descriptor_sets,
                                             {});
    // TODO(dk6): Maybe rework to run compute shaders once per frame
    for (auto &[pipeline, draw] : scene->_draws) {
      uint32_t instances_number = static_cast<uint32_t>(draw.instances_data_buffer_data.size());
      uint32_t culling_push_contants[] {
        draw.meshes_data_buffer_id,
        draw.instances_data_buffer_id,
        instances_number,

        scene->_counters_buffer_id,

        scene->_transforms_buffer_id,
        scene->_render_transforms_buffer_id,

        scene->camera_buffer_id(),
        scene->_bound_boxes_buffer_id,
        _depth_pyramid_image_id,
      };
      _culling_command_unit->pushConstants(_instances_culling_pipeline.layout(), vk::ShaderStageFlagBits::eCompute,
                                          0, sizeof(culling_push_contants), culling_push_contants);

      _culling_command_unit->dispatch(calculate_work_groups_number(instances_number, culling_work_group_size), 1, 1);
    }
  }

  vk::BufferMemoryBarrier instances_count_culling_barrier {
    .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
    // Write are also exists on next shader...
    // Maybe it is correct to have different buffers for instances counters and draws counters
    // But now it works anyway)
    .dstAccessMask = vk::AccessFlagBits::eShaderRead,
    .buffer = scene->_counters_buffer.buffer(),
    .offset = 0,
    .size = scene->_counters_buffer.byte_size(),
  };

  _culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                        vk::PipelineStageFlagBits::eComputeShader,
                                        {}, {}, {instances_count_culling_barrier}, {});

  // ===== Setup and call instances collect shader =====
  _culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _instances_collect_pipeline.pipeline());

  _culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                           {_instances_collect_pipeline.layout()},
                                           bindless_set_number,
                                           culling_descriptor_sets,
                                           {});

  for (auto &[pipeline, draw] : scene->_draws) {
    uint32_t max_draws_count = static_cast<uint32_t>(draw.meshes_data_buffer_data.size());
    uint32_t culling_push_contants[] {
      max_draws_count,
      draw.meshes_data_buffer_id,
      draw.draw_commands_buffer_id,
      draw.meshes_render_info_id,

      scene->_counters_buffer_id,
      draw.draw_counter_index,
    };

    _culling_command_unit->pushConstants(_instances_collect_pipeline.layout(), vk::ShaderStageFlagBits::eCompute,
                                        0, sizeof(culling_push_contants), culling_push_contants);

    _culling_command_unit->dispatch(calculate_work_groups_number(max_draws_count, culling_work_group_size), 1, 1);
  }

  _culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                       _timestamps_query_pool.get(),
                                       enum_cast(Timestamp::CullingEnd));

  _culling_command_unit.end();
  if (scene->_was_transfer_in_this_frame) {
    _culling_command_unit.add_wait_semaphore(scene->_transfers_semaphore.get(),
                                             vk::PipelineStageFlagBits::eComputeShader);
  }
  _culling_command_unit.add_signal_semaphore(_culling_semaphore.get());
  vk::SubmitInfo culling_submit_info = _culling_command_unit.submit_info();
  _state->queue().submit(culling_submit_info);
}

void mr::RenderContext::late_culling_geometry(const SceneHandle scene)
{
  _late_culling_command_unit.begin();

  build_depth_pyramid();

  _late_culling_command_unit->resetQueryPool(_timestamps_query_pool.get(), enum_cast(Timestamp::LateCullingStart), 2);

  _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe,
                                       _timestamps_query_pool.get(),
                                       enum_cast(Timestamp::LateCullingStart));

  // ===== Fill all counters by zeroes =====
  _late_culling_command_unit->fillBuffer(scene->_counters_buffer.buffer(), 0, scene->_counters_buffer.byte_size(), 0);
  vk::BufferMemoryBarrier set_count_to_zero_barrier {
    .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
    .dstAccessMask = vk::AccessFlagBits::eShaderRead,
    .buffer = scene->_counters_buffer.buffer(),
    .offset = 0,
    .size = scene->_counters_buffer.byte_size(),
  };
  _late_culling_command_unit->pipelineBarrier(
    vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
    {}, {set_count_to_zero_barrier}, {});

  std::array culling_descriptor_sets {_bindless_set.set()};

  // ===== Setup and call culling instances shader =====
  if (not is_render_option_enabled(_render_options, RenderOptions::DisableCulling)) {
    _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _late_instances_culling_pipeline.pipeline());

    _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                             {_late_instances_culling_pipeline.layout()},
                                             bindless_set_number,
                                             culling_descriptor_sets,
                                             {});
    // TODO(dk6): Maybe rework to run compute shaders once per frame
    for (auto &[pipeline, draw] : scene->_draws) {
      uint32_t instances_number = static_cast<uint32_t>(draw.instances_data_buffer_data.size());
      uint32_t culling_push_contants[] {
        draw.meshes_data_buffer_id,
        draw.instances_data_buffer_id,
        instances_number,

        scene->_counters_buffer_id,

        scene->_transforms_buffer_id,
        scene->_render_transforms_buffer_id,

        scene->camera_buffer_id(),
        scene->_bound_boxes_buffer_id,

        _depth_pyramid.mip_levels_number(),
        _depth_pyramid_shader_data_buffer_id,
        _depth_pyramid_extent.width,
        _depth_pyramid_extent.height,
      };
      _late_culling_command_unit->pushConstants(_late_instances_culling_pipeline.layout(), vk::ShaderStageFlagBits::eCompute,
                                          0, sizeof(culling_push_contants), culling_push_contants);

      _late_culling_command_unit->dispatch(calculate_work_groups_number(instances_number, culling_work_group_size), 1, 1);
    }
  }

  vk::BufferMemoryBarrier instances_count_culling_barrier {
    .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
    // Write are also exists on next shader...
    // Maybe it is correct to have different buffers for instances counters and draws counters
    // But now it works anyway)
    .dstAccessMask = vk::AccessFlagBits::eShaderRead,
    .buffer = scene->_counters_buffer.buffer(),
    .offset = 0,
    .size = scene->_counters_buffer.byte_size(),
  };

  _late_culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                        vk::PipelineStageFlagBits::eComputeShader,
                                        {}, {}, {instances_count_culling_barrier}, {});

  // ===== Setup and call instances collect shader =====
  _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _instances_collect_pipeline.pipeline());

  _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                           {_instances_collect_pipeline.layout()},
                                           bindless_set_number,
                                           culling_descriptor_sets,
                                           {});

  for (auto &[pipeline, draw] : scene->_draws) {
    uint32_t max_draws_count = static_cast<uint32_t>(draw.meshes_data_buffer_data.size());
    uint32_t culling_push_contants[] {
      max_draws_count,
      draw.meshes_data_buffer_id,
      draw.draw_commands_buffer_id,
      draw.meshes_render_info_id,

      scene->_counters_buffer_id,
      draw.draw_counter_index,
    };

    _late_culling_command_unit->pushConstants(_instances_collect_pipeline.layout(), vk::ShaderStageFlagBits::eCompute,
                                        0, sizeof(culling_push_contants), culling_push_contants);

    _late_culling_command_unit->dispatch(calculate_work_groups_number(max_draws_count, culling_work_group_size), 1, 1);
  }

  _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                       _timestamps_query_pool.get(),
                                       enum_cast(Timestamp::LateCullingEnd));

  _late_culling_command_unit.end();

  _late_culling_command_unit.add_wait_semaphore(_visible_models_rendering_semaphore.get(),
                                           vk::PipelineStageFlagBits::eComputeShader);
  _late_culling_command_unit.add_signal_semaphore(_late_culling_semaphore.get());

  vk::SubmitInfo culling_submit_info = _late_culling_command_unit.submit_info();
  _state->queue().submit(culling_submit_info);
}


void mr::RenderContext::update_bound_boxes_data()
{
  if (not _bound_boxes_draw_enabled) {
    return;
  }

  uint32_t bound_boxes_number = _bound_boxes_data.size();
  if (_bound_boxes_data_dirty) {
    _bound_boxes_buffer.write(_pre_model_layout_transition_command_unit, std::as_bytes(std::span(_bound_boxes_data)));
    _bound_boxes_data_dirty = false;

    vk::BufferMemoryBarrier bound_boxes_data_barrier {
      .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = _bound_boxes_buffer.buffer(),
      .offset = 0,
      .size = bound_boxes_number * sizeof(BoundBoxRenderData),
    };

    _pre_model_layout_transition_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                                               vk::PipelineStageFlagBits::eGeometryShader,
                                                               {}, {}, {bound_boxes_data_barrier}, {});
  }
}

void mr::RenderContext::render_bound_boxes(const SceneHandle scene)
{
  if (not _bound_boxes_draw_enabled) {
    return;
  }

  uint32_t bound_boxes_number = _bound_boxes_data.size();

  _models_command_unit->bindPipeline(vk::PipelineBindPoint::eGraphics, _bound_boxes_draw_pipeline.pipeline());

  _models_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                           {_bound_boxes_draw_pipeline.layout()},
                                           bindless_set_number,
                                           {_bindless_set.set()},
                                           {});

  uint32_t bound_boxes_push_contants[] {
    scene->camera_buffer_id(),
    _bound_boxes_buffer_id,
    (uint32_t)scene->_draw_bound_rects,
  };
  _models_command_unit->pushConstants(_bound_boxes_draw_pipeline.layout(),
                                      vk::ShaderStageFlagBits::eAllGraphics,
                                      0, sizeof(bound_boxes_push_contants), bound_boxes_push_contants);

  _models_command_unit->draw(1, bound_boxes_number, 0, 0);
}

void mr::RenderContext::render_geometry(const SceneHandle scene, bool is_late_pass)
{
  ZoneScoped;

  CommandUnit &cmd_unit = is_late_pass ? _late_models_command_unit : _models_command_unit;
  CommandUnit &trans_cmd_unit = is_late_pass ? _pre_model_layout_transition_command_unit_late
                                             : _pre_model_layout_transition_command_unit;

  cmd_unit.begin();

  TRACY_VK_ZONE_BEGIN() {
    TracyVkZone(_models_tracy_gpu_context, cmd_unit.command_buffer(), "Models pass GPU");


    trans_cmd_unit.begin();
    if (not is_late_pass) {
      for (auto &gbuf : _gbuffers) {
        gbuf.switch_layout(trans_cmd_unit, vk::ImageLayout::eColorAttachmentOptimal);
      }
    }
    _depthbuffer.switch_layout(trans_cmd_unit, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    if (not is_late_pass) {
      update_bound_boxes_data();
    }

    auto trans_sem = is_late_pass ? _pre_model_layout_transition_semaphore_late.get()
                                  : _pre_model_layout_transition_semaphore.get();
    trans_cmd_unit.end();
    trans_cmd_unit.add_signal_semaphore(trans_sem);

    cmd_unit.add_wait_semaphore(trans_sem, vk::PipelineStageFlagBits::eVertexShader);

    auto culling_sem = is_late_pass ? _late_culling_semaphore.get() : _culling_semaphore.get();
    cmd_unit.add_wait_semaphore(culling_sem, vk::PipelineStageFlagBits::eVertexShader);

    auto gbufs_attachs = _gbuffers | std::views::transform([](const ColorAttachmentImage &gbuf) {
      return gbuf.attachment_info();
    }) | std::ranges::to<InplaceVector<vk::RenderingAttachmentInfoKHR, gbuffers_number>>();
    auto depth_attachment_info = _depthbuffer.attachment_info();

    if (is_late_pass) {
      for (auto &attach : gbufs_attachs) {
        attach.loadOp = vk::AttachmentLoadOp::eLoad;
      }
      depth_attachment_info.loadOp = vk::AttachmentLoadOp::eLoad;
    }

    vk::RenderingInfoKHR attachment_info {
      .renderArea = { 0, 0, _extent.width, _extent.height },
      .layerCount = 1,
      .colorAttachmentCount = static_cast<uint32_t>(gbufs_attachs.size()),
      .pColorAttachments = gbufs_attachs.data(),
      .pDepthAttachment = &depth_attachment_info,
      // .pStencilAttachment = &depth_attachment_info,
    };

    cmd_unit->resetQueryPool(_timestamps_query_pool.get(), enum_cast(Timestamp::ModelsStart), 2);

    cmd_unit->beginRendering(&attachment_info);

    vk::Viewport viewport {
      .x = 0, .y = 0,
      .width = static_cast<float>(_extent.width),
      .height = static_cast<float>(_extent.height),
      .minDepth = 0, .maxDepth = 1,
    };
    cmd_unit->setViewport(0, viewport);

    vk::Rect2D scissors {
      .offset = {0, 0},
      .extent = {
        static_cast<uint32_t>(_extent.width),
        static_cast<uint32_t>(_extent.height),
      },
    };
    cmd_unit->setScissor(0, scissors);

    if (not is_late_pass) {
      render_bound_boxes(scene);
    }

    render_models(scene, cmd_unit);

    cmd_unit->endRendering();
  }

  TracyVkCollect(_models_tracy_gpu_context, cmd_unit.command_buffer());

  auto finish_semaphore = is_late_pass ? _models_render_finished_semaphore.get()
                                       : _visible_models_rendering_semaphore.get();
  // finish_semaphore = _models_render_finished_semaphore.get();
  cmd_unit.add_signal_semaphore(finish_semaphore);

  cmd_unit.end();

  vk::SubmitInfo pre_model_layout_transition_submit_info =
    trans_cmd_unit.submit_info();
  _state->queue().submit(pre_model_layout_transition_submit_info);

  vk::SubmitInfo models_submit_info = cmd_unit.submit_info();
  _state->queue().submit(models_submit_info);
}

void mr::RenderContext::build_depth_pyramid()
{
  _late_culling_command_unit->resetQueryPool(_timestamps_query_pool.get(), enum_cast(Timestamp::BuildDepthPyramidStart), 2);

  _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                       _timestamps_query_pool.get(),
                                       enum_cast(Timestamp::BuildDepthPyramidStart));

  _depthbuffer.switch_layout(_late_culling_command_unit, vk::ImageLayout::eGeneral);

  _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _depth_pyramid_pipeline.pipeline());
  _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                           {_depth_pyramid_pipeline.layout()},
                                           bindless_set_number,
                                           {_bindless_set.set()},
                                           {});

  auto dst_size = _depth_pyramid_extent;
  for (uint32_t i = 0; dst_size.width > 0 && dst_size.height > 0; i++) {
    uint32_t depth_pyramid_push_contants[] {
      i == 0 ? _depth_image_attacment_id : _depth_pyramid_mips[i - 1], // src image
      _depth_pyramid_mips[i], // dst image
      dst_size.width,
      dst_size.height,
    };

    _late_culling_command_unit->pushConstants({_depth_pyramid_pipeline.layout()}, vk::ShaderStageFlagBits::eCompute,
                                        0, sizeof(depth_pyramid_push_contants), depth_pyramid_push_contants);

    uint32_t work_group_width = calculate_work_groups_number(dst_size.width, culling_work_group_size);
    uint32_t work_group_height = calculate_work_groups_number(dst_size.height, culling_work_group_size);
    _late_culling_command_unit->dispatch(work_group_width, work_group_height, 1);

    // just for pipeline barrier
    _depth_pyramid.switch_layout(_late_culling_command_unit, vk::ImageLayout::eGeneral, i, 1, true);

    dst_size.width /= 2;
    dst_size.height /= 2;
  }

  _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                       _timestamps_query_pool.get(),
                                       enum_cast(Timestamp::BuildDepthPyramidEnd));
}

void mr::RenderContext::resize(const mr::Extent &extent)
{
  _extent = extent;
  _depth_pyramid_extent = Extent(extent.width / 2, extent.height / 2);
}

void mr::RenderContext::render(const SceneHandle scene, Presenter &presenter)
{
  ASSERT(this == &scene->render_context());
  ASSERT(this == &presenter.render_context());

  ZoneScoped;
  const char *frame_name = "render";
  FrameMarkStart(frame_name);

  auto render_start_time = ClockT::now();

  _state->device().waitForFences(_image_fence.get(), VK_TRUE, UINT64_MAX);
  _state->device().resetFences(_image_fence.get());

  resize(presenter.extent());
  // NOTE: Camera UBO is already updated and this resize will only affect next frame
  scene->_camera.cam().projection().resize((float)_extent.width / _extent.height);

  // ------------------------------------------------
  // Frusum culling of previously visible objects
  // ------------------------------------------------

  culling_geometry(scene);

  // ------------------------------------------------
  // Rendering of previously visible objects
  // ------------------------------------------------

  auto cpu_models_start_time = ClockT::now();
  render_geometry(scene, false);
  auto cpu_models_time = ClockT::now() - cpu_models_start_time;

  // ------------------------------------------------
  // Late frusum culling and occlusion culling
  // ------------------------------------------------

  late_culling_geometry(scene);

  // ------------------------------------------------
  // Rendering previously invisible models
  // ------------------------------------------------

  render_geometry(scene, true);

  // --------------------------------------------------------------------------
  // Lights shading pass
  // --------------------------------------------------------------------------

  auto cpu_shading_start_time = ClockT::now();
  render_lights(scene, presenter);
  auto cpu_shading_time = ClockT::now() - cpu_shading_start_time;

  _lights_command_unit.add_wait_semaphore(_models_render_finished_semaphore.get(),
                                          vk::PipelineStageFlagBits::eColorAttachmentOutput);
  auto image_available_semaphore = presenter.image_available_semaphore();
  if (image_available_semaphore) {
    _lights_command_unit.add_wait_semaphore(image_available_semaphore,
                                            vk::PipelineStageFlagBits::eColorAttachmentOutput);
  }
  auto render_finished_semaphore = presenter.render_finished_semaphore();
  if (render_finished_semaphore) {
    _lights_command_unit.add_signal_semaphore(render_finished_semaphore);
  }

  vk::SubmitInfo pre_light_layout_transition_submit_info =
    _pre_light_layout_transition_command_unit.submit_info();
  _state->queue().submit(pre_light_layout_transition_submit_info);

  vk::SubmitInfo light_submit_info = _lights_command_unit.submit_info();
  _state->queue().submit(light_submit_info, _image_fence.get());

  presenter.present();

  FrameMarkEnd(frame_name);

  calculate_stat(scene, render_start_time, ClockT::now(), cpu_models_time, cpu_shading_time);
}

void mr::RenderContext::calculate_stat(SceneHandle scene,
                                       ClockT::time_point render_start_time,
                                       ClockT::time_point render_finish_time,
                                       ClockT::duration models_time,
                                       ClockT::duration shading_time)
{
  auto get_ms = [](ClockT::duration time) -> double {
    using namespace std::chrono;
    return duration_cast<duration<double, std::milli>>(time).count();
  };
  _render_stat.render_cpu_time_ms = get_ms(render_finish_time - render_start_time);
  _render_stat.cpu_time_ms = get_ms(render_start_time - _prev_start_time);
  _render_stat.cpu_fps = 1000 / _render_stat.cpu_time_ms;

  _prev_start_time = render_start_time;

  _render_stat.models_cpu_time_ms = get_ms(models_time);
  _render_stat.shading_cpu_time_ms = get_ms(models_time);

// #define QUERY_RES_WAIT
#ifdef QUERY_RES_WAIT
  struct QueryRes { uint64_t first; };
  vk::QueryResultFlags query_res_flags = vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait;
#else // QUERY_RES_WAIT
  using QueryRes = std::pair<uint64_t, uint32_t>;
  vk::QueryResultFlags query_res_flags = vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability;
#endif // QUERY_RES_WAIT

  std::array<QueryRes, timestamps_number> timestamps;
  _state->device().getQueryPoolResults(_timestamps_query_pool.get(),
                                       0, timestamps_number,
                                       sizeof(timestamps[0]) * timestamps_number,
                                       timestamps.data(),
                                       sizeof(timestamps[0]), // stride
                                       query_res_flags);


// TODO(dk6): maybe validate results for not wait
// #ifndef QUERY_RES_WAIT
//   bool timestamps_valid = true;
//   for (const auto& ts : timestamps) {
//     if (ts.second == 0) { // Not available
//       return;
//     }
//   }
// #endif

  _render_stat.culling_gpu_time_ms = (timestamps[enum_cast(Timestamp::CullingEnd)].first -
                                      timestamps[enum_cast(Timestamp::CullingStart)].first) * _timestamp_to_ms;
  _render_stat.models_gpu_time_ms = (timestamps[enum_cast(Timestamp::ModelsEnd)].first -
                                     timestamps[enum_cast(Timestamp::ModelsStart)].first) * _timestamp_to_ms;
  _render_stat.build_depth_pyramid_gpu_time_ms =
    (timestamps[enum_cast(Timestamp::BuildDepthPyramidEnd)].first -
     timestamps[enum_cast(Timestamp::BuildDepthPyramidStart)].first) * _timestamp_to_ms;
  _render_stat.late_culling_gpu_time_ms = (timestamps[enum_cast(Timestamp::LateCullingEnd)].first -
                                           timestamps[enum_cast(Timestamp::LateCullingStart)].first) * _timestamp_to_ms;
  _render_stat.shading_gpu_time_ms = (timestamps[enum_cast(Timestamp::ShadingEnd)].first -
                                      timestamps[enum_cast(Timestamp::ShadingStart)].first) * _timestamp_to_ms;

  // TODO(dk6): calculate using formula end(timestamps) - start(timestamps)
  // _render_stat.render_gpu_time_ms = _render_stat.models_gpu_time_ms + _render_stat.shading_gpu_time_ms;
  _render_stat.render_gpu_time_ms = (timestamps[enum_cast(Timestamp::TimestampsNumber) - 1].first -
                                     timestamps[0].first) * _timestamp_to_ms;

  _render_stat.gpu_time_ms = (timestamps[0].first - _prev_first_timestamp) * _timestamp_to_ms;
  _prev_first_timestamp = timestamps[0].first;
  _render_stat.gpu_fps = 1000 / _render_stat.gpu_time_ms;

  _render_stat.triangles_number = scene->_triangles_number.load();
  _render_stat.vertexes_number = scene->_vertexes_number.load();
}

void mr::RenderStat::write_to_json(std::ostream &out) const noexcept
{
  double triangles_per_second = triangles_number / (gpu_time_ms / 1000);
  std::println(out, "{{");
  std::println(out, "  \"cpu_fps\": {:.2f},", cpu_fps);
  std::println(out, "  \"cpu_time_ms\": {:.2f},", cpu_time_ms);
  std::println(out, "  \"gpu_fps\": {:.2f},", gpu_fps);
  std::println(out, "  \"gpu_time_ms\": {:.2f},", gpu_time_ms);
  std::println(out, "  \"cpu_rendering_time_ms\": {:.2f},", render_cpu_time_ms);
  std::println(out, "  \"cpu_models_time_ms\": {:.2f},", models_cpu_time_ms);
  std::println(out, "  \"cpu_shading_time_ms\": {:.2f},", shading_cpu_time_ms);
  std::println(out, "  \"culling_gpu_time_ms\": {:.3f},", culling_gpu_time_ms);
  std::println(out, "  \"build_depth_pyramid_gpu_time_ms\": {:.3f},", build_depth_pyramid_gpu_time_ms);
  std::println(out, "  \"late_culling_gpu_time_ms\": {:.3f},", late_culling_gpu_time_ms);
  std::println(out, "  \"gpu_rendering_time_ms\": {:.2f},", render_gpu_time_ms);
  std::println(out, "  \"gpu_models_time_ms\": {:.2f},", models_gpu_time_ms);
  std::println(out, "  \"gpu_shading_time_ms\": {:.2f},", shading_gpu_time_ms);
  std::println(out, "  \"triangles_per_second\": {:.3f},", triangles_per_second);
  std::println(out, "  \"triangles_per_second_millions\": {:.3f},", triangles_per_second * 1e-6);
  std::println(out, "  \"triangles_number\": {},", triangles_number);
  std::println(out, "  \"vertexes_number\": {}", vertexes_number);
  std::println(out, "}}");
}
