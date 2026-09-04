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
#include <fstream>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

#define TRACY_VK_ZONE_BEGIN()

static mr::Extent calculate_depth_pyramid_extent(mr::Extent screen_extent) {
  return {std::bit_floor(screen_extent.width), std::bit_floor(screen_extent.height)};
}

static uint32_t msoc_tile_mip_level(uint32_t tile_size) {
  return static_cast<uint32_t>(std::countr_zero(tile_size));
}

mr::RenderContext::RenderContext(VulkanGlobalState *global_state, Extent extent, RenderContextConfig config)
  : _state(std::make_shared<VulkanState>(global_state))
  , _config(config)
  , _render_bounds_state(config.bounds_state)
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
  , _transfer_command_unit(*_state)
  , _extent(extent)
  , _depthbuffer(*_state, _extent)
  , _image_fence (_state->device().createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}).value)
  , _default_descriptor_allocator(*_state)
  , _vertex_buffers_heap(default_vertex_number, 1)
  , _positions_vertex_buffer(*_state, default_vertex_number * position_bytes_size)
  , _attributes_vertex_buffer(*_state, default_vertex_number * attributes_bytes_size)
  , _index_buffer(*_state, default_index_number * sizeof(uint32_t), sizeof(uint32_t))
  , _depth_pyramid_extent(calculate_depth_pyramid_extent(_extent))
  , _depth_pyramid(*_state, _depth_pyramid_extent, vk::Format::eR32Sfloat,
                   calculate_mips_levels_number(_depth_pyramid_extent))
{
  // Temporary restriction: MSOC shaders currently assume square power-of-two tiles.
  ASSERT(_config.msoc_tile_size.width == _config.msoc_tile_size.height);
  ASSERT(std::has_single_bit(_config.msoc_tile_size.width));

  std::println("render_bounds_state: {}", enum_cast(_render_bounds_state));

  if (is_render_option_enabled(_config.options, RenderOptions::DisableCulling) &&
      not is_render_option_enabled(_config.options, RenderOptions::DisableOcclusionCulling)) {
    _config.options |= RenderOptions::DisableOcclusionCulling;
    MR_WARNING("Disabling frustum culling also disable occlusion culling");
  }

  for (auto _ : std::views::iota(0, gbuffers_number)) {
    _gbuffers.emplace_back(*_state, _extent, vk::Format::eR32G32B32A32Sfloat);
  }
  _models_render_finished_semaphore = _state->device().createSemaphoreUnique({}).value;

  if (is_render_option_enabled(_config.options, RenderOptions::CollectPosInstanceId)) {
    _gbuffers_data_copy_ready_semaphore = _state->device().createSemaphoreUnique({}).value;
    _position_instance_copy_cmd_unit = CommandUnit(*_state);
    auto &pos_gbuf = _gbuffers[enum_cast(GBuffer::Position)];
    _position_instance_id_stage_buffer = HostBuffer(*_state, pos_gbuf.size(), vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eHostCached);
    _position_instance_copy_fence = _state->device().createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}).value;
  }

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
  std::array<ShaderResourceView, gbuffers_number> shader_resources;
  for (int i = 0; i < gbuffers_number; i++) {
    shader_resources[i] = ShaderResourceView(i, &_gbuffers[i]);
  }

  _lights_render_data.lights_descriptor_set.update(*_state, std::span(shader_resources.data(), gbuffers_number));

  boost::unordered_map<std::string, std::string> defines {
    {"TEXTURES_BINDING",        std::to_string(textures_binding)},
    {"UNIFORM_BUFFERS_BINDING", std::to_string(uniform_buffer_binding)},
    {"STORAGE_BUFFERS_BINDING", std::to_string(storage_buffer_binding)},
  };

  if (is_render_option_enabled(_config.options, RenderOptions::HashColoring)) {
    defines["HASH_COLORING"] = "ON";
  }

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
    // TODO(dk6): rename to sampled_images
    {"TEXTURES_BINDING",        std::to_string(textures_binding)},
    {"UNIFORM_BUFFERS_BINDING", std::to_string(uniform_buffer_binding)},
    {"STORAGE_BUFFERS_BINDING", std::to_string(storage_buffer_binding)},
    {"STORAGE_IMAGES_BINDING", std::to_string(storage_images_binding)},
    {"BINDLESS_SET", std::to_string(bindless_set_number)},
    {"THREADS_NUM", std::to_string(culling_work_group_size)},
    {"SCAN_BLOCKS_LOCAL_SIZE_X", std::to_string(scan_blocks_local_size_x)},
    {"SCAN_BLOCK_SIZE", std::to_string(scan_block_size)},
  };
  if (is_render_option_enabled(_config.options, RenderOptions::DisableCulling)) {
    defines.insert({"DISABLE_CULLING", "ON"});
  }
  if (is_render_option_enabled(_config.options, RenderOptions::EnableCullingStats)) {
    defines.insert({"COLLECT_CULLING_STAT", "ON"});
  }
  if (is_render_option_enabled(_config.options, RenderOptions::OcDrawAlways)) {
    defines.insert({"OC_DRAW_ALWAYS", "ON"});
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
  _instances_collect_mark_shader = ResourceManager<Shader>::get().create("InstancesCollectMarkShader",
    *_state, "culling/instances_collect/mark_visible", defines);
  _instances_collect_mark_pipeline = ComputePipeline(*_state, _instances_collect_mark_shader, set_layouts);
  _instances_collect_scan_blocks_shader = ResourceManager<Shader>::get().create("InstancesCollectScanBlocksShader",
    *_state, "culling/exclusive_scan/scan_blocks", defines);
  _instances_collect_scan_blocks_pipeline =
    ComputePipeline(*_state, _instances_collect_scan_blocks_shader, set_layouts);
  _instances_collect_add_offsets_shader = ResourceManager<Shader>::get().create("InstancesCollectAddOffsetsShader",
    *_state, "culling/exclusive_scan/add_offsets", defines);
  _instances_collect_add_offsets_pipeline =
    ComputePipeline(*_state, _instances_collect_add_offsets_shader, set_layouts);

  // ---------------------------
  // Depth pyramid
  // ---------------------------

  _depth_pyramid_shader = ResourceManager<Shader>::get().create("DepthPyramidBuild",
    *_state, "culling/depth_pyramid_build", defines);
  _depth_pyramid_pipeline = ComputePipeline(*_state, _depth_pyramid_shader, set_layouts);

  _depth_sampler = Sampler(*_state, vk::Filter::eLinear, vk::SamplerMipmapMode::eNearest,
                                    vk::SamplerAddressMode::eClampToEdge, vk::SamplerReductionMode::eMax,
                                   _depth_pyramid.mip_levels_number());

  // This resource don't unregister in destructor becase bindless set die with them
  _depthbuffer_resource = ShaderImageResource {
    .image = &_depthbuffer,
    .sampler = &_depth_sampler,
    .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
  };
  _depth_image_attacment_id = _bindless_set.register_resource(&_depthbuffer_resource);

  for (uint32_t mip = 0; mip < _depth_pyramid.mip_levels_number(); mip++) {
    DepthPyramidMip &pyramid_mip = _depth_pyramid_mips.emplace_back();

    pyramid_mip.sampled_image_resource = ShaderPyramidImageLevelResource {
      .image = &_depth_pyramid,
      .mip_level = mip,
      .sampler = &_depth_sampler,
      .layout = vk::ImageLayout::eShaderReadOnlyOptimal
    };
    pyramid_mip.descriptor_sampled_image_id = _bindless_set.register_resource(&pyramid_mip.sampled_image_resource);

    pyramid_mip.storage_image_resource = ShaderPyramidImageLevelResource {
      .image = &_depth_pyramid,
      .mip_level = mip,
      .sampler = nullptr,
      .layout = vk::ImageLayout::eGeneral,
    };
    pyramid_mip.descriptor_storage_image_id = _bindless_set.register_resource(&pyramid_mip.storage_image_resource);
  }

  // ---------------------------
  // Occlusion culling
  // ---------------------------

  _depth_pyramid_resource = ShaderImageResource {
    .image = &_depth_pyramid,
    .sampler = &_depth_sampler,
    .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
  };
  _depth_pyramid_image_id = _bindless_set.register_resource(&_depth_pyramid_resource);

  _depth_pyramid_mips_scale_coefs_buffer = StorageBuffer(*_state, sizeof(float) * depth_pyramid_max_levels * 2);
  _depth_pyramid_mips_scale_coefs_buffer_id = _bindless_set.register_resource(&_depth_pyramid_mips_scale_coefs_buffer);

  // ---------------------------
  // Late culling
  // ---------------------------

  defines["MAX_DEPTH_PYRAMID_LEVELS"] = std::to_string(depth_pyramid_max_levels);

  if (_config.oc_type == OcclusionCullingType::TwoPhaseHiZ) {
    auto hiz_defines = defines;
    hiz_defines["OC_TYPE"] = std::to_string(enum_cast(OcclusionCullingType::TwoPhaseHiZ));
    hiz_defines["HIZ_OC_TYPE"] = std::to_string(enum_cast(OcclusionCullingType::TwoPhaseHiZ));
    hiz_defines["MSOC_OC_TYPE"] = std::to_string(enum_cast(OcclusionCullingType::MaskedSoftware));
    hiz_defines["MSOC_ADAPTIVE_TILE_SIZE_OC_TYPE"] =
      std::to_string(enum_cast(OcclusionCullingType::MsocAdaptiveTileSize));
    hiz_defines["BOUNDS_TYPE"] = std::to_string(enum_cast(_config.oc_bounds));
    hiz_defines["BOUNDS_TYPE_BOXES"] = std::to_string(enum_cast(OcclusionCullingBounds::Box));
    hiz_defines["BOUNDS_TYPE_SPHERES"] = std::to_string(enum_cast(OcclusionCullingBounds::Sphere));
    hiz_defines["BOUNDS_TYPE_DYNAMIC_BEST"] = std::to_string(enum_cast(OcclusionCullingBounds::DynamicBest));
    hiz_defines["BOUNDS_TYPE_BOTH"] = std::to_string(enum_cast(OcclusionCullingBounds::Both));

    _late_instances_culling_shader = ResourceManager<Shader>::get().create("LateInstancesCullingShader",
      *_state, "culling/late_instances_culling/hiz", hiz_defines);
    _late_instances_culling_pipeline = ComputePipeline(*_state, _late_instances_culling_shader, set_layouts);
  } else if (uses_msoc_pipeline(_config.oc_type)) {
    auto msoc_defines = defines;
    msoc_defines["OC_TYPE"] = std::to_string(enum_cast(_config.oc_type));
    msoc_defines["HIZ_OC_TYPE"] = std::to_string(enum_cast(OcclusionCullingType::TwoPhaseHiZ));
    msoc_defines["MSOC_OC_TYPE"] = std::to_string(enum_cast(OcclusionCullingType::MaskedSoftware));
    msoc_defines["MSOC_ADAPTIVE_TILE_SIZE_OC_TYPE"] =
      std::to_string(enum_cast(OcclusionCullingType::MsocAdaptiveTileSize));
    msoc_defines["BOUNDS_TYPE"] = std::to_string(enum_cast(_config.oc_bounds));
    msoc_defines["BOUNDS_TYPE_BOXES"] = std::to_string(enum_cast(OcclusionCullingBounds::Box));
    msoc_defines["BOUNDS_TYPE_SPHERES"] = std::to_string(enum_cast(OcclusionCullingBounds::Sphere));
    msoc_defines["BOUNDS_TYPE_DYNAMIC_BEST"] = std::to_string(enum_cast(OcclusionCullingBounds::DynamicBest));
    msoc_defines["BOUNDS_TYPE_BOTH"] = std::to_string(enum_cast(OcclusionCullingBounds::Both));
    msoc_defines["TILE_SIZE"] = std::to_string(_config.msoc_tile_size.width);
    msoc_defines["TILES_PER_THREAD"] = std::to_string(_config.msoc_tiles_per_thread);

    const bool msoc_coarse_hiz =
      is_render_option_enabled(_config.options, RenderOptions::MsocWithHizCoarseCheck);
    const bool msoc_adaptive_tile = _config.oc_type == OcclusionCullingType::MsocAdaptiveTileSize;

    // Fixed MSOC tiles must match the mip texel grid; adaptive keeps exact AABB.
    if (!msoc_adaptive_tile) {
      msoc_defines["MSOC_SNAP_RECT_TO_TILE_GRID"] = "ON";
    }

    if (msoc_coarse_hiz) {
      _msoc_tile_prep_coarse_hiz_shader = ResourceManager<Shader>::get().create("MsocTilePrepCoarseHizShader",
        *_state, "culling/late_instances_culling/msoc/tile_prep_coarse_hiz", msoc_defines);
      _msoc_tile_prep_coarse_hiz_pipeline =
        ComputePipeline(*_state, _msoc_tile_prep_coarse_hiz_shader, set_layouts);
    } else {
      _msoc_tile_prep_shader = ResourceManager<Shader>::get().create("MsocTilePrepShader",
        *_state, "culling/late_instances_culling/msoc/tile_prep", msoc_defines);
      _msoc_tile_prep_pipeline = ComputePipeline(*_state, _msoc_tile_prep_shader, set_layouts);
    }

    if (!msoc_adaptive_tile) {
      _msoc_tiled_sampler = Sampler(*_state, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest,
                                    vk::SamplerAddressMode::eClampToEdge, 1);
      const uint32_t tile_mip = msoc_tile_mip_level(_config.msoc_tile_size.width);
      _msoc_tiled_depth_resource = ShaderPyramidImageLevelResource {
        .image = &_depth_pyramid,
        .mip_level = tile_mip,
        .sampler = &_msoc_tiled_sampler,
        .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
      };
      _tiled_buffer_image_id = _bindless_set.register_resource(&_msoc_tiled_depth_resource);
    }

    auto msoc_tile_test_defines = msoc_defines;
    if (_config.test_tile_early_exit) {
      msoc_tile_test_defines["TEST_TILES_EARLY_EXIT"] = "ON";
    }
    if (msoc_coarse_hiz) {
      msoc_tile_test_defines["MSOC_WITH_HIZ_COARSE_CHECK"] = "ON";
    }

    if (msoc_adaptive_tile) {
      msoc_tile_test_defines["TILE_MIP_LEVEL"] =
        std::to_string(msoc_tile_mip_level(_config.msoc_tile_size.width));
      _msoc_tile_test_adaptive_shader = ResourceManager<Shader>::get().create("MsocTileTestAdaptiveShader",
        *_state, "culling/late_instances_culling/msoc/tile_test_adaptive_size", msoc_tile_test_defines);
      _msoc_tile_test_adaptive_pipeline =
        ComputePipeline(*_state, _msoc_tile_test_adaptive_shader, set_layouts);
    } else {
      _msoc_tile_test_shader = ResourceManager<Shader>::get().create("MsocTileTestShader",
        *_state, "culling/late_instances_culling/msoc/tile_test", msoc_tile_test_defines);
      _msoc_tile_test_pipeline = ComputePipeline(*_state, _msoc_tile_test_shader, set_layouts);
    }

    _msoc_tile_apply_shader = ResourceManager<Shader>::get().create("MsocTileApplyShader",
      *_state, "culling/late_instances_culling/msoc/tile_apply", msoc_defines);
    _msoc_tile_apply_pipeline = ComputePipeline(*_state, _msoc_tile_apply_shader, set_layouts);

    _msoc_tile_finalize_shader = ResourceManager<Shader>::get().create("MsocTileFinalizeShader",
      *_state, "culling/late_instances_culling/msoc/tile_finalize", msoc_defines);
    _msoc_tile_finalize_pipeline = ComputePipeline(*_state, _msoc_tile_finalize_shader, set_layouts);
  }

  // ---------------------------
  // Stats
  // ---------------------------

  if (is_render_option_enabled(_config.options, RenderOptions::EnableCullingStats)) {
    _culling_stat_buffer = StorageBuffer(*_state, sizeof(CullingStats), vk::BufferUsageFlagBits::eTransferSrc);
    _culling_stat_buffer_id = _bindless_set.register_resource(&_culling_stat_buffer);
    _culling_stat_stage_buffer = HostBuffer(*_state, sizeof(CullingStats), vk::BufferUsageFlagBits::eTransferDst);
  }

  if (is_render_option_enabled(_config.options, RenderOptions::EnableCullingVisualiztion)) {
    _copy_visibility_states_shader = ResourceManager<Shader>::get().create("CopyVisibility",
      *_state, "culling/copy_visibility", defines);
    _copy_visibility_states_pipeline = ComputePipeline(*_state, _copy_visibility_states_shader, set_layouts);

    _copy_on_screen_shader = ResourceManager<Shader>::get().create("CopyOnScreen",
      *_state, "culling/copy_visibility/copy_visible_from_gbuf", defines);
    _copy_on_screen_pipeline = ComputePipeline(*_state, _copy_on_screen_shader, set_layouts);
    _read_from_gbuf_sampler = Sampler(*_state, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest,
                                      vk::SamplerAddressMode::eClampToEdge);
    // This resource don't unregister in destructor becase bindless set die with them
    _read_from_gbuf_resource = ShaderImageResource {
      .image = &_gbuffers[enum_cast(GBuffer::Position)],
      .sampler = &_read_from_gbuf_sampler,
      .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    _sampled_gbuffer_id = _bindless_set.register_resource(&_read_from_gbuf_resource);
    _clear_on_screen_state_shader = ResourceManager<Shader>::get().create("ClearOnScreenStates",
      *_state, "culling/copy_visibility/clear_on_screen_states", defines);
    _clear_on_screen_state_pipeline = ComputePipeline(*_state, _clear_on_screen_state_shader, set_layouts);

  }
}

void mr::RenderContext::init_bound_box_rendering()
{
  boost::unordered_map<std::string, std::string> defines {
    {"TEXTURES_BINDING",        std::to_string(textures_binding)},
    {"UNIFORM_BUFFERS_BINDING", std::to_string(uniform_buffer_binding)},
    {"STORAGE_BUFFERS_BINDING", std::to_string(storage_buffer_binding)},
    {"BINDLESS_SET", std::to_string(bindless_set_number)},
    {"ENABLE_BOUNDS_FRAG_COLOR", "ON"}, // this can cause DEVICE_LOST on some GPUs
    {"STATE_DISABLE", std::to_string(enum_cast(RenderBoundsState::Disable))},
    {"STATE_BOUND_BOXES", std::to_string(enum_cast(RenderBoundsState::BoundBoxes))},
    {"STATE_BOUND_BOX_RECTANGLES", std::to_string(enum_cast(RenderBoundsState::BoundBoxRectangles))},
    {"STATE_BOUND_SPHERES", std::to_string(enum_cast(RenderBoundsState::BoundSpheres))},
    {"STATE_BOUND_SPHERE_RECTANGLES", std::to_string(enum_cast(RenderBoundsState::BoundSphereRectangles))},
    {"STATE_DYNAMIC_BEST", std::to_string(enum_cast(RenderBoundsState::DynamicBest))},
    {"STATE_DYNAMIC_BEST_RECTANGLES", std::to_string(enum_cast(RenderBoundsState::DynamicBestRectangles))},
  };
  _bound_boxes_draw_shader = ResourceManager<Shader>::get().create("BoundBoxShader", *_state, "bound_box", defines);

  std::array set_layouts {
    _converted_bindless_set_layout,
  };
  _bound_boxes_draw_pipeline =
    GraphicsPipeline(*this, GraphicsPipeline::Subpass::OpaqueGeometry, _bound_boxes_draw_shader, {}, set_layouts);

  // TODO(dk6): use dynamic buffer
  _bound_boxes_buffer = StorageBuffer(*_state, sizeof(BoundBoxRenderData) * 1'000'000);
  _bound_boxes_buffer_id = _bindless_set.register_resource(&_bound_boxes_buffer);
}

void mr::RenderContext::draw_bound_box(uint32_t transforms_buffer_id, uint32_t transform_index,
                                       uint32_t bound_boxes_buffer_id, uint32_t bound_spheres_buffer_id,
                                       uint32_t bound_box_index) noexcept
{
  _bound_boxes_data.emplace_back(BoundBoxRenderData {
    .transforms_buffer_id = transforms_buffer_id,
    .transform_index = transform_index,
    .bound_boxes_buffer_id = bound_boxes_buffer_id,
    .bound_spheres_buffer_id = bound_spheres_buffer_id,
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

  auto positions_data = vbufs_data[0];
  auto attributes_data = vbufs_data[1];
  std::vector<std::byte> attributes_fake_data;
  // TODO(dk6): temporary solution for loading models without attributes
  if (attributes_data.size() == 0) {
    attributes_fake_data.resize(attributes_bytes_size * positions_data.size() / position_bytes_size);
    attributes_data = attributes_fake_data;
  }

  ASSERT(positions_data.size() % position_bytes_size == 0);
  ASSERT(attributes_data.size() % attributes_bytes_size == 0);

  ASSERT(positions_data.size() / position_bytes_size == attributes_data.size() / attributes_bytes_size);
  VkDeviceSize vertexes_number = positions_data.size() / position_bytes_size;

  auto alloc_info = _vertex_buffers_heap.allocate(vertexes_number);
  if (alloc_info.resized) {
    _positions_vertex_buffer.resize(command_unit, _vertex_buffers_heap.size() * position_bytes_size);
    _attributes_vertex_buffer.resize(command_unit, _vertex_buffers_heap.size() * attributes_bytes_size);
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
    is_render_option_enabled(_config.options, RenderOptions::EnableVsync));
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

void mr::RenderContext::render_models(const SceneHandle scene, CommandUnit &cmd_unit, bool /*is_late_pass*/)
{

  std::array<vk::Buffer, 2> vertex_buffers {
    _positions_vertex_buffer.buffer(),
    _attributes_vertex_buffer.buffer(),
  };
  std::array<vk::DeviceSize, vertex_buffers.size()> vertex_buffers_offsets {0, 0};
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
      scene->transforms_buffer_id(),
      scene->camera_buffer_id(),
      scene->_occluded_instances_state_buffer_id,
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
}

void mr::RenderContext::run_exclusive_scan(CommandUnit &command_unit,
                                           std::span<const vk::DescriptorSet> culling_descriptor_sets,
                                           uint32_t elements_count,
                                           uint32_t input_buffer_id,
                                           uint32_t output_buffer_id,
                                           const StorageBuffer &output_buffer,
                                           const std::array<uint32_t, 3> &scan_aux_buffer_ids,
                                           const std::array<StorageBuffer, 3> &scan_aux_buffers)
{
  command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _instances_collect_scan_blocks_pipeline.pipeline());
  command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                   {_instances_collect_scan_blocks_pipeline.layout()},
                                   bindless_set_number,
                                   culling_descriptor_sets,
                                   {});

  struct ScanLevelState {
    uint32_t elements_count = 0;
    uint32_t output_buffer_id = BindlessDescriptorSet::invalid_id;
    const StorageBuffer *output_buffer = nullptr;
    uint32_t aux_buffer_id = BindlessDescriptorSet::invalid_id;
    const StorageBuffer *aux_buffer = nullptr;
    uint32_t groups_count = 0;
  };

  std::array<ScanLevelState, 3> level_states {};
  uint32_t level = 0;
  uint32_t level_input_buffer_id = input_buffer_id;
  uint32_t level_elements_count = elements_count;
  while (true) {
    auto &state = level_states[level];
    state.elements_count = level_elements_count;
    if (level == 0) {
      state.output_buffer_id = output_buffer_id;
      state.output_buffer = &output_buffer;
      state.aux_buffer_id = scan_aux_buffer_ids[0];
      state.aux_buffer = &scan_aux_buffers[0];
    } else if (level == 1) {
      state.output_buffer_id = scan_aux_buffer_ids[1];
      state.output_buffer = &scan_aux_buffers[1];
      state.aux_buffer_id = scan_aux_buffer_ids[2];
      state.aux_buffer = &scan_aux_buffers[2];
    } else {
      state.output_buffer_id = scan_aux_buffer_ids[0];
      state.output_buffer = &scan_aux_buffers[0];
      state.aux_buffer_id = BindlessDescriptorSet::invalid_id;
      state.aux_buffer = nullptr;
    }

    uint32_t scan_push_constants[] {
      state.elements_count,
      level_input_buffer_id,
      state.output_buffer_id,
      state.aux_buffer_id,
    };

    command_unit->pushConstants(_instances_collect_scan_blocks_pipeline.layout(),
                                vk::ShaderStageFlagBits::eCompute,
                                0, sizeof(scan_push_constants), scan_push_constants);
    state.groups_count = calculate_work_groups_number(level_elements_count, scan_block_size);
    command_unit->dispatch(state.groups_count, 1, 1);

    std::array<vk::BufferMemoryBarrier, 2> barriers {
      vk::BufferMemoryBarrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .buffer = state.output_buffer->buffer(),
        .offset = 0,
        .size = state.output_buffer->byte_size(),
      },
      vk::BufferMemoryBarrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .buffer = state.aux_buffer ? state.aux_buffer->buffer() : vk::Buffer {},
        .offset = 0,
        .size = state.aux_buffer ? state.aux_buffer->byte_size() : 0,
      },
    };
    if (state.aux_buffer) {
      command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                    vk::PipelineStageFlagBits::eComputeShader,
                                    {}, {}, barriers, {});
    } else {
      command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                    vk::PipelineStageFlagBits::eComputeShader,
                                    {}, {}, {barriers[0]}, {});
    }

    if (state.groups_count <= 1 || level == level_states.size() - 1) {
      break;
    }
    level_input_buffer_id = state.aux_buffer_id;
    level_elements_count = state.groups_count;
    level++;
  }

  if (level == 0) {
    return;
  }

  command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _instances_collect_add_offsets_pipeline.pipeline());
  command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                   {_instances_collect_add_offsets_pipeline.layout()},
                                   bindless_set_number,
                                   culling_descriptor_sets,
                                   {});

  while (level > 0) {
    auto &dst_state = level_states[level - 1];
    auto &src_state = level_states[level];
    uint32_t add_offsets_push_constants[] {
      dst_state.elements_count,
      dst_state.output_buffer_id,
      src_state.output_buffer_id,
    };
    command_unit->pushConstants(_instances_collect_add_offsets_pipeline.layout(),
                                vk::ShaderStageFlagBits::eCompute,
                                0, sizeof(add_offsets_push_constants), add_offsets_push_constants);
    command_unit->dispatch(calculate_work_groups_number(dst_state.elements_count, scan_block_size), 1, 1);

    if (level > 1) {
      vk::BufferMemoryBarrier add_level_barrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .buffer = dst_state.output_buffer->buffer(),
        .offset = 0,
        .size = dst_state.output_buffer->byte_size(),
      };
      command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                    vk::PipelineStageFlagBits::eComputeShader,
                                    {}, {}, {add_level_barrier}, {});
    }

    level--;
  }
}

void mr::RenderContext::run_msoc_tile_prep(const SceneHandle scene,
                                           Scene::MeshesWithSamePipeline &draw,
                                           uint32_t instances_number,
                                           std::span<const vk::DescriptorSet> culling_descriptor_sets)
{
  ASSERT(draw.msoc_buffers.has_value());
  auto &msoc = draw.msoc_buffers.value();

  _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _msoc_tile_prep_pipeline.pipeline());
  _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                 {_msoc_tile_prep_pipeline.layout()},
                                                 bindless_set_number,
                                                 culling_descriptor_sets,
                                                 {});

  uint32_t prep_push_constants[] {
    draw.meshes_data_buffer_id,
    draw.instances_data_buffer_id,
    instances_number,
    scene->_transforms_buffer_id,
    scene->camera_buffer_id(),
    scene->_bound_boxes_buffer_id,
    scene->_bound_spheres_buffer_id,
    _extent.width,
    _extent.height,
    msoc.tile_count_buffer_id,
    msoc.tile_dims_buffer_id,
    msoc.screen_rect_buffer_id,
    msoc.query_depth_buffer_id,
    _culling_stat_buffer_id,
  };
  _late_culling_command_unit->pushConstants(_msoc_tile_prep_pipeline.layout(),
                                            vk::ShaderStageFlagBits::eCompute,
                                            0, sizeof(prep_push_constants), prep_push_constants);
  _late_culling_command_unit->dispatch(calculate_work_groups_number(instances_number, culling_work_group_size), 1, 1);

  std::array<vk::BufferMemoryBarrier, 4> prep_barriers {
    vk::BufferMemoryBarrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = msoc.tile_count_buffer.buffer(),
      .offset = 0,
      .size = msoc.tile_count_buffer.byte_size(),
    },
    vk::BufferMemoryBarrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = msoc.tile_dims_buffer.buffer(),
      .offset = 0,
      .size = msoc.tile_dims_buffer.byte_size(),
    },
    vk::BufferMemoryBarrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = msoc.screen_rect_buffer.buffer(),
      .offset = 0,
      .size = msoc.screen_rect_buffer.byte_size(),
    },
    vk::BufferMemoryBarrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = msoc.query_depth_buffer.buffer(),
      .offset = 0,
      .size = msoc.query_depth_buffer.byte_size(),
    },
  };
  _late_culling_command_unit->pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
    {}, prep_barriers, {});
}

void mr::RenderContext::run_msoc_tile_prep_coarse_hiz(const SceneHandle scene,
                                                      Scene::MeshesWithSamePipeline &draw,
                                                      uint32_t instances_number,
                                                      std::span<const vk::DescriptorSet> culling_descriptor_sets)
{
  ASSERT(draw.msoc_buffers.has_value());
  auto &msoc = draw.msoc_buffers.value();

  _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute,
                                           _msoc_tile_prep_coarse_hiz_pipeline.pipeline());
  _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                 {_msoc_tile_prep_coarse_hiz_pipeline.layout()},
                                                 bindless_set_number,
                                                 culling_descriptor_sets,
                                                 {});

  uint32_t prep_push_constants[] {
    draw.meshes_data_buffer_id,
    draw.instances_data_buffer_id,
    instances_number,
    scene->_transforms_buffer_id,
    scene->camera_buffer_id(),
    scene->_bound_boxes_buffer_id,
    scene->_bound_spheres_buffer_id,
    _extent.width,
    _extent.height,
    _depth_pyramid.mip_levels_number(),
    _depth_pyramid_extent.width,
    _depth_pyramid_extent.height,
    _depth_pyramid_image_id,
    _depth_pyramid_mips_scale_coefs_buffer_id,
    msoc.tile_count_buffer_id,
    msoc.tile_dims_buffer_id,
    msoc.screen_rect_buffer_id,
    msoc.query_depth_buffer_id,
    _culling_stat_buffer_id,
  };
  _late_culling_command_unit->pushConstants(_msoc_tile_prep_coarse_hiz_pipeline.layout(),
                                            vk::ShaderStageFlagBits::eCompute,
                                            0, sizeof(prep_push_constants), prep_push_constants);
  _late_culling_command_unit->dispatch(calculate_work_groups_number(instances_number, culling_work_group_size), 1, 1);

  std::array<vk::BufferMemoryBarrier, 5> prep_barriers {
    vk::BufferMemoryBarrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = msoc.tile_count_buffer.buffer(),
      .offset = 0,
      .size = msoc.tile_count_buffer.byte_size(),
    },
    vk::BufferMemoryBarrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = msoc.tile_dims_buffer.buffer(),
      .offset = 0,
      .size = msoc.tile_dims_buffer.byte_size(),
    },
    vk::BufferMemoryBarrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = msoc.screen_rect_buffer.buffer(),
      .offset = 0,
      .size = msoc.screen_rect_buffer.byte_size(),
    },
    vk::BufferMemoryBarrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = msoc.query_depth_buffer.buffer(),
      .offset = 0,
      .size = msoc.query_depth_buffer.byte_size(),
    },
    vk::BufferMemoryBarrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = draw.instances_data_buffer.buffer(),
      .offset = 0,
      .size = draw.instances_data_buffer.byte_size(),
    },
  };
  _late_culling_command_unit->pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
    {}, prep_barriers, {});
}

void mr::RenderContext::run_msoc_tile_scan(const SceneHandle /*scene*/,
                                           Scene::MeshesWithSamePipeline &draw,
                                           uint32_t instances_number,
                                           std::span<const vk::DescriptorSet> culling_descriptor_sets)
{
  ASSERT(draw.msoc_buffers.has_value());
  auto &msoc = draw.msoc_buffers.value();

  run_exclusive_scan(_late_culling_command_unit,
                     culling_descriptor_sets,
                     instances_number,
                     msoc.tile_count_buffer_id,
                     msoc.tile_offset_buffer_id,
                     msoc.tile_offset_buffer,
                     draw.scan_aux_buffer_ids,
                     draw.scan_aux_buffers);

  vk::BufferMemoryBarrier scan_barrier {
    .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
    .dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
    .buffer = msoc.tile_offset_buffer.buffer(),
    .offset = 0,
    .size = msoc.tile_offset_buffer.byte_size(),
  };
  _late_culling_command_unit->pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
    {}, {scan_barrier}, {});
}

void mr::RenderContext::run_msoc_tile_finalize(const SceneHandle scene,
                                               Scene::MeshesWithSamePipeline &draw,
                                               uint32_t instances_number,
                                               std::span<const vk::DescriptorSet> culling_descriptor_sets)
{
  ASSERT(draw.msoc_buffers.has_value());
  auto &msoc = draw.msoc_buffers.value();

  _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _msoc_tile_finalize_pipeline.pipeline());
  _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                 {_msoc_tile_finalize_pipeline.layout()},
                                                 bindless_set_number,
                                                 culling_descriptor_sets,
                                                 {});
  uint32_t finalize_push_constants[] {
    instances_number,
    msoc.tile_count_buffer_id,
    msoc.tile_offset_buffer_id,
    scene->_counters_buffer_id,
    msoc.msoc_dispatch_cmd_index,
    _culling_stat_buffer_id,
  };
  _late_culling_command_unit->pushConstants(_msoc_tile_finalize_pipeline.layout(),
                                            vk::ShaderStageFlagBits::eCompute,
                                            0, sizeof(finalize_push_constants), finalize_push_constants);
  _late_culling_command_unit->dispatch(1, 1, 1);

  std::array<vk::BufferMemoryBarrier, 2> finalize_barriers {
    vk::BufferMemoryBarrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = msoc.tile_offset_buffer.buffer(),
      .offset = 0,
      .size = msoc.tile_offset_buffer.byte_size(),
    },
    vk::BufferMemoryBarrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eIndirectCommandRead,
      .buffer = scene->_counters_buffer.buffer(),
      .offset = static_cast<vk::DeviceSize>(msoc.msoc_dispatch_cmd_index * sizeof(uint32_t)),
      .size = sizeof(vk::DispatchIndirectCommand),
    },
  };
  _late_culling_command_unit->pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
    {}, {finalize_barriers[0]}, {});
  _late_culling_command_unit->pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {},
    {}, {finalize_barriers[1]}, {});
}

void mr::RenderContext::run_msoc_tile_test(const SceneHandle scene,
                                           Scene::MeshesWithSamePipeline &draw,
                                           uint32_t instances_number,
                                           std::span<const vk::DescriptorSet> culling_descriptor_sets)
{
  ASSERT(draw.msoc_buffers.has_value());
  auto &msoc = draw.msoc_buffers.value();

  const bool msoc_adaptive_tile = _config.oc_type == OcclusionCullingType::MsocAdaptiveTileSize;

  if (!msoc_adaptive_tile) {
    _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _msoc_tile_test_pipeline.pipeline());
    _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                   {_msoc_tile_test_pipeline.layout()},
                                                   bindless_set_number,
                                                   culling_descriptor_sets,
                                                   {});
    struct MsocTileTestPushConstants {
      uint32_t instances_number;
      uint32_t tiled_buffer_image_id;
      float tiled_buffer_scale_x;
      float tiled_buffer_scale_y;
      uint32_t screen_width;
      uint32_t screen_height;
      uint32_t tile_offset_buffer_id;
      uint32_t tile_dims_buffer_id;
      uint32_t screen_rect_buffer_id;
      uint32_t query_depth_buffer_id;
      uint32_t visible_flag_buffer_id;
    } test_push_constants {
      .instances_number = instances_number,
      .tiled_buffer_image_id = _tiled_buffer_image_id,
      .tiled_buffer_scale_x = _tiled_buffer_scale[0],
      .tiled_buffer_scale_y = _tiled_buffer_scale[1],
      .screen_width = _extent.width,
      .screen_height = _extent.height,
      .tile_offset_buffer_id = msoc.tile_offset_buffer_id,
      .tile_dims_buffer_id = msoc.tile_dims_buffer_id,
      .screen_rect_buffer_id = msoc.screen_rect_buffer_id,
      .query_depth_buffer_id = msoc.query_depth_buffer_id,
      .visible_flag_buffer_id = msoc.visible_flag_buffer_id,
    };
    _late_culling_command_unit->pushConstants(_msoc_tile_test_pipeline.layout(),
                                              vk::ShaderStageFlagBits::eCompute,
                                              0, sizeof(test_push_constants), &test_push_constants);
  } else {
    _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute,
                                             _msoc_tile_test_adaptive_pipeline.pipeline());
    _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                   {_msoc_tile_test_adaptive_pipeline.layout()},
                                                   bindless_set_number,
                                                   culling_descriptor_sets,
                                                   {});
    struct MsocTileTestAdaptivePushConstants {
      uint32_t instances_number;
      uint32_t depth_pyramid_image_id;
      uint32_t depth_pyramid_mips_scales_buffer_id;
      uint32_t depth_pyramid_width;
      uint32_t depth_pyramid_height;
      uint32_t tile_offset_buffer_id;
      uint32_t tile_dims_buffer_id;
      uint32_t screen_rect_buffer_id;
      uint32_t query_depth_buffer_id;
      uint32_t visible_flag_buffer_id;
    } test_push_constants {
      .instances_number = instances_number,
      .depth_pyramid_image_id = _depth_pyramid_image_id,
      .depth_pyramid_mips_scales_buffer_id = _depth_pyramid_mips_scale_coefs_buffer_id,
      .depth_pyramid_width = _depth_pyramid_extent.width,
      .depth_pyramid_height = _depth_pyramid_extent.height,
      .tile_offset_buffer_id = msoc.tile_offset_buffer_id,
      .tile_dims_buffer_id = msoc.tile_dims_buffer_id,
      .screen_rect_buffer_id = msoc.screen_rect_buffer_id,
      .query_depth_buffer_id = msoc.query_depth_buffer_id,
      .visible_flag_buffer_id = msoc.visible_flag_buffer_id,
    };
    _late_culling_command_unit->pushConstants(_msoc_tile_test_adaptive_pipeline.layout(),
                                              vk::ShaderStageFlagBits::eCompute,
                                              0, sizeof(test_push_constants), &test_push_constants);
  }

  _late_culling_command_unit->dispatchIndirect(
    scene->_counters_buffer.buffer(),
    static_cast<vk::DeviceSize>(msoc.msoc_dispatch_cmd_index * sizeof(uint32_t)));

  vk::BufferMemoryBarrier test_barrier {
    .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
    .dstAccessMask = vk::AccessFlagBits::eShaderRead,
    .buffer = msoc.visible_flag_buffer.buffer(),
    .offset = 0,
    .size = msoc.visible_flag_buffer.byte_size(),
  };
  _late_culling_command_unit->pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
    {}, {test_barrier}, {});
}

void mr::RenderContext::run_msoc_tile_apply(const SceneHandle scene,
                                            Scene::MeshesWithSamePipeline &draw,
                                            uint32_t instances_number,
                                            std::span<const vk::DescriptorSet> culling_descriptor_sets)
{
  ASSERT(draw.msoc_buffers.has_value());
  auto &msoc = draw.msoc_buffers.value();

  _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _msoc_tile_apply_pipeline.pipeline());
  _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                 {_msoc_tile_apply_pipeline.layout()},
                                                 bindless_set_number,
                                                 culling_descriptor_sets,
                                                 {});
  uint32_t apply_push_constants[] {
    draw.meshes_data_buffer_id,
    draw.instances_data_buffer_id,
    instances_number,
    scene->_counters_buffer_id,
    scene->_transforms_buffer_id,
    scene->camera_buffer_id(),
    scene->_bound_boxes_buffer_id,
    scene->_bound_spheres_buffer_id,
    msoc.tile_count_buffer_id,
    msoc.visible_flag_buffer_id,
    _culling_stat_buffer_id,
  };
  _late_culling_command_unit->pushConstants(_msoc_tile_apply_pipeline.layout(),
                                            vk::ShaderStageFlagBits::eCompute,
                                            0, sizeof(apply_push_constants), apply_push_constants);
  _late_culling_command_unit->dispatch(calculate_work_groups_number(instances_number, culling_work_group_size), 1, 1);
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

      scene->camera_buffer_id(),
      scene->_bound_boxes_buffer_id,
    };
    _culling_command_unit->pushConstants(_instances_culling_pipeline.layout(), vk::ShaderStageFlagBits::eCompute,
                                        0, sizeof(culling_push_contants), culling_push_contants);

    _culling_command_unit->dispatch(calculate_work_groups_number(instances_number, culling_work_group_size), 1, 1);
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

  // ===== Build compact draw list =====
  bool disable_culling = is_render_option_enabled(_config.options, RenderOptions::DisableCulling);
  for (auto &[pipeline, draw] : scene->_draws) {
    uint32_t max_draws_count = static_cast<uint32_t>(draw.meshes_data_buffer_data.size());
    if (!disable_culling) {
      uint32_t collect_mark_push_constants[] {
        max_draws_count,
        draw.meshes_data_buffer_id,
        scene->_counters_buffer_id,
        draw.draw_visibility_buffer_id,
      };

      _culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _instances_collect_mark_pipeline.pipeline());
      _culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                {_instances_collect_mark_pipeline.layout()},
                                                bindless_set_number,
                                                culling_descriptor_sets,
                                                {});
      _culling_command_unit->pushConstants(_instances_collect_mark_pipeline.layout(), vk::ShaderStageFlagBits::eCompute,
                                           0, sizeof(collect_mark_push_constants), collect_mark_push_constants);
      _culling_command_unit->dispatch(calculate_work_groups_number(max_draws_count, culling_work_group_size), 1, 1);

      vk::BufferMemoryBarrier mark_to_prefix_barrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .buffer = draw.draw_visibility_buffer.buffer(),
        .offset = 0,
        .size = draw.draw_visibility_buffer.byte_size(),
      };
      _culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                             vk::PipelineStageFlagBits::eComputeShader,
                                             {}, {}, {mark_to_prefix_barrier}, {});

      run_exclusive_scan(_culling_command_unit,
                         std::span(culling_descriptor_sets),
                         max_draws_count,
                         draw.draw_visibility_buffer_id,
                         draw.draw_prefix_buffer_id,
                         draw.draw_prefix_buffer,
                         draw.scan_aux_buffer_ids,
                         draw.scan_aux_buffers);

      vk::BufferMemoryBarrier prefix_to_count_barrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .buffer = draw.draw_prefix_buffer.buffer(),
        .offset = 0,
        .size = draw.draw_prefix_buffer.byte_size(),
      };
      _culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                             vk::PipelineStageFlagBits::eComputeShader,
                                             {}, {}, {prefix_to_count_barrier}, {});

      vk::BufferMemoryBarrier prefix_to_scatter_barrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .buffer = draw.draw_prefix_buffer.buffer(),
        .offset = 0,
        .size = draw.draw_prefix_buffer.byte_size(),
      };
      _culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                             vk::PipelineStageFlagBits::eComputeShader,
                                             {}, {}, {prefix_to_scatter_barrier}, {});
    }

    uint32_t collect_scatter_push_constants[] {
      max_draws_count,
      draw.meshes_data_buffer_id,
      draw.draw_commands_buffer_id,
      draw.meshes_render_info_id,
      scene->_counters_buffer_id,
      draw.draw_counter_index,
      draw.draw_visibility_buffer_id,
      draw.draw_prefix_buffer_id,
    };
    _culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _instances_collect_pipeline.pipeline());
    _culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                              {_instances_collect_pipeline.layout()},
                                              bindless_set_number,
                                              culling_descriptor_sets,
                                              {});
    _culling_command_unit->pushConstants(_instances_collect_pipeline.layout(), vk::ShaderStageFlagBits::eCompute,
                                         0, sizeof(collect_scatter_push_constants), collect_scatter_push_constants);
    _culling_command_unit->dispatch(calculate_work_groups_number(max_draws_count, culling_work_group_size), 1, 1);
  }

  _culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                       _timestamps_query_pool.get(),
                                       enum_cast(Timestamp::CullingEnd));

  // --------------------------------------------------------------------------
  // Culling visualization: copy invisible, but considered visible objects
  // --------------------------------------------------------------------------

  if (is_render_option_enabled(_config.options, RenderOptions::EnableCullingVisualiztion)) {
    if (_save_culling_visualization || _clear_culling_visualization) {
      // ---------------------------------------
      // Copy previous data
      // ---------------------------------------
      _culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _clear_on_screen_state_pipeline.pipeline());
      _culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                {_copy_on_screen_pipeline.layout()},
                                                bindless_set_number,
                                                culling_descriptor_sets,
                                                {});

      uint32_t transforms_number = static_cast<uint32_t>(scene->_transforms_data.size());
      uint32_t clear_states_push_constants[] {
        scene->_occluded_instances_state_buffer_id,
        transforms_number,
        (uint32_t) _clear_culling_visualization,
      };
      _culling_command_unit->pushConstants(_clear_on_screen_state_pipeline.layout(),
                                           vk::ShaderStageFlagBits::eCompute,
                                           0, sizeof(clear_states_push_constants),
                                           clear_states_push_constants);

      _culling_command_unit->dispatch(calculate_work_groups_number(transforms_number, culling_work_group_size), 1, 1);

      vk::BufferMemoryBarrier visibility_states_barrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .buffer = scene->_occluded_instances_state_buffer.buffer(),
        .offset = 0,
        .size = scene->_occluded_instances_state_buffer.byte_size(),
      };
      _culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                                  vk::PipelineStageFlagBits::eComputeShader,
                                                  {}, {}, {visibility_states_barrier}, {});


      // ---------------------------------------
      // Copy from last frame gbuffer
      // ---------------------------------------
      if (_save_culling_visualization) {
        _culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _copy_on_screen_pipeline.pipeline());
        _culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                  {_copy_on_screen_pipeline.layout()},
                                                  bindless_set_number,
                                                  culling_descriptor_sets,
                                                  {});

        auto &gbuf = _gbuffers[enum_cast(GBuffer::Position)];
        auto [width, heigth, _z] = gbuf.extent();
        uint32_t read_on_screen_visibility_push_constants[] {
          _sampled_gbuffer_id,
          width, heigth,
          scene->_occluded_instances_state_buffer_id,
        };
        _culling_command_unit->pushConstants(_copy_on_screen_pipeline.layout(),
                                             vk::ShaderStageFlagBits::eCompute,
                                             0, sizeof(read_on_screen_visibility_push_constants),
                                             read_on_screen_visibility_push_constants);

        gbuf.switch_layout(_culling_command_unit, vk::ImageLayout::eShaderReadOnlyOptimal);
        _culling_command_unit->dispatch(
          calculate_work_groups_number(width, culling_work_group_size),
          calculate_work_groups_number(heigth, culling_work_group_size),
          1
        );

        // TODO(dk6): maybe use barrier?
        // vk::BufferMemoryBarrier visibility_states_barrier {
        //   .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        //   .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        //   .buffer = scene->_occluded_instances_state_buffer.buffer(),
        //   .offset = 0,
        //   .size = scene->_occluded_instances_state_buffer.byte_size(),
        // };
        // _late_culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
        //                                             vk::PipelineStageFlagBits::eDrawIndirect,
        //                                             {}, {}, {visibility_states_barrier}, {});
      }
    }
  }

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

  _depth_pyramid_mips_scale_coefs_buffer.write(
    _late_culling_command_unit, std::span(_depth_pyramid_mips_scale_coefs_buffer_data));

  // TODO(dk6): add barrier for _depth_pyramid_mips_scale_coefs_buffer

  build_depth_pyramid();

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

  if (is_render_option_enabled(_config.options, RenderOptions::EnableCullingStats)) {
    _late_culling_command_unit->fillBuffer(_culling_stat_buffer.buffer(), 0, _culling_stat_buffer.byte_size(), 0);
    vk::BufferMemoryBarrier set_culling_stat_to_zero_barrier {
      .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
      .dstAccessMask = vk::AccessFlagBits::eShaderRead,
      .buffer = _culling_stat_buffer.buffer(),
      .offset = 0,
      .size = _culling_stat_buffer.byte_size(),
    };
    _late_culling_command_unit->pipelineBarrier(
      vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
      {}, {set_culling_stat_to_zero_barrier}, {});
  }

  std::array culling_descriptor_sets {_bindless_set.set()};

  // ===== Setup and call culling instances shader =====
  if (not is_render_option_enabled(_config.options, RenderOptions::DisableCulling)) {
    _late_culling_command_unit->resetQueryPool(_timestamps_query_pool.get(),
                                               enum_cast(Timestamp::LateCullingStart), 2);
    if (uses_msoc_pipeline(_config.oc_type)) {
      constexpr uint32_t msoc_timestamps_number =
        enum_cast(Timestamp::MsocTileApplyEnd) - enum_cast(Timestamp::MsocTilePrepStart) + 1;
      _late_culling_command_unit->resetQueryPool(_timestamps_query_pool.get(),
                                                 enum_cast(Timestamp::MsocTilePrepStart),
                                                 msoc_timestamps_number);
    }

    _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                               _timestamps_query_pool.get(),
                                               enum_cast(Timestamp::LateCullingStart));

    if (_config.oc_type == OcclusionCullingType::TwoPhaseHiZ) {
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

          scene->camera_buffer_id(),
          scene->_bound_boxes_buffer_id,
          scene->_bound_spheres_buffer_id,

          _depth_pyramid.mip_levels_number(),
          _depth_pyramid_extent.width,
          _depth_pyramid_extent.height,

          _depth_pyramid_image_id,
          _depth_pyramid_mips_scale_coefs_buffer_id,

          // if EnableCullingStats option not enabled this number is not initializated and must not be used in shader
          _culling_stat_buffer_id,
        };
        _late_culling_command_unit->pushConstants(_late_instances_culling_pipeline.layout(),
                                                  vk::ShaderStageFlagBits::eCompute,
                                                  0, sizeof(culling_push_contants), culling_push_contants);

        _late_culling_command_unit->dispatch(calculate_work_groups_number(instances_number, culling_work_group_size), 1, 1);
      }
    } else if (uses_msoc_pipeline(_config.oc_type)) {
      _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                                 _timestamps_query_pool.get(),
                                                 enum_cast(Timestamp::MsocTilePrepStart));
      for (auto &[pipeline, draw] : scene->_draws) {
        uint32_t instances_number = static_cast<uint32_t>(draw.instances_data_buffer_data.size());
        ASSERT(draw.msoc_buffers.has_value());
        auto &msoc = draw.msoc_buffers.value();
        _late_culling_command_unit->fillBuffer(msoc.visible_flag_buffer.buffer(), 0,
                                               msoc.visible_flag_buffer.byte_size(), 0);
        vk::BufferMemoryBarrier zero_visible_flags_barrier {
          .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
          .dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
          .buffer = msoc.visible_flag_buffer.buffer(),
          .offset = 0,
          .size = msoc.visible_flag_buffer.byte_size(),
        };
        _late_culling_command_unit->pipelineBarrier(
          vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
          {}, {zero_visible_flags_barrier}, {});

        if (is_render_option_enabled(_config.options, RenderOptions::MsocWithHizCoarseCheck)) {
          run_msoc_tile_prep_coarse_hiz(scene, draw, instances_number, culling_descriptor_sets);
        } else {
          run_msoc_tile_prep(scene, draw, instances_number, culling_descriptor_sets);
        }
      }
      _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                                 _timestamps_query_pool.get(),
                                                 enum_cast(Timestamp::MsocTileScanStart));
      for (auto &[pipeline, draw] : scene->_draws) {
        uint32_t instances_number = static_cast<uint32_t>(draw.instances_data_buffer_data.size());
        run_msoc_tile_scan(scene, draw, instances_number, culling_descriptor_sets);
      }
      _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                                 _timestamps_query_pool.get(),
                                                 enum_cast(Timestamp::MsocTileFinalizeStart));
      for (auto &[pipeline, draw] : scene->_draws) {
        uint32_t instances_number = static_cast<uint32_t>(draw.instances_data_buffer_data.size());
        run_msoc_tile_finalize(scene, draw, instances_number, culling_descriptor_sets);
      }
      // Must match finalize's DrawIndirect barrier and tile_test dispatchIndirect.
      _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eDrawIndirect,
                                                 _timestamps_query_pool.get(),
                                                 enum_cast(Timestamp::MsocTileTestStart));
      for (auto &[pipeline, draw] : scene->_draws) {
        uint32_t instances_number = static_cast<uint32_t>(draw.instances_data_buffer_data.size());
        run_msoc_tile_test(scene, draw, instances_number, culling_descriptor_sets);
      }
      _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                                 _timestamps_query_pool.get(),
                                                 enum_cast(Timestamp::MsocTileApplyStart));
      for (auto &[pipeline, draw] : scene->_draws) {
        uint32_t instances_number = static_cast<uint32_t>(draw.instances_data_buffer_data.size());
        run_msoc_tile_apply(scene, draw, instances_number, culling_descriptor_sets);
      }
      _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                                 _timestamps_query_pool.get(),
                                                 enum_cast(Timestamp::MsocTileApplyEnd));
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

  // ===== Build compact draw list =====
  bool disable_culling = is_render_option_enabled(_config.options, RenderOptions::DisableCulling);
  for (auto &[pipeline, draw] : scene->_draws) {
    uint32_t max_draws_count = static_cast<uint32_t>(draw.meshes_data_buffer_data.size());
    if (!disable_culling) {
      uint32_t collect_mark_push_constants[] {
        max_draws_count,
        draw.meshes_data_buffer_id,
        scene->_counters_buffer_id,
        draw.draw_visibility_buffer_id,
      };
      _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _instances_collect_mark_pipeline.pipeline());
      _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                     {_instances_collect_mark_pipeline.layout()},
                                                     bindless_set_number,
                                                     culling_descriptor_sets,
                                                     {});
      _late_culling_command_unit->pushConstants(_instances_collect_mark_pipeline.layout(), vk::ShaderStageFlagBits::eCompute,
                                                0, sizeof(collect_mark_push_constants), collect_mark_push_constants);
      _late_culling_command_unit->dispatch(calculate_work_groups_number(max_draws_count, culling_work_group_size), 1, 1);

      vk::BufferMemoryBarrier mark_to_prefix_barrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .buffer = draw.draw_visibility_buffer.buffer(),
        .offset = 0,
        .size = draw.draw_visibility_buffer.byte_size(),
      };
      _late_culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                                  vk::PipelineStageFlagBits::eComputeShader,
                                                  {}, {}, {mark_to_prefix_barrier}, {});

      run_exclusive_scan(_late_culling_command_unit,
                         std::span(culling_descriptor_sets),
                         max_draws_count,
                         draw.draw_visibility_buffer_id,
                         draw.draw_prefix_buffer_id,
                         draw.draw_prefix_buffer,
                         draw.scan_aux_buffer_ids,
                         draw.scan_aux_buffers);

      vk::BufferMemoryBarrier prefix_to_count_barrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .buffer = draw.draw_prefix_buffer.buffer(),
        .offset = 0,
        .size = draw.draw_prefix_buffer.byte_size(),
      };
      _late_culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                                  vk::PipelineStageFlagBits::eComputeShader,
                                                  {}, {}, {prefix_to_count_barrier}, {});

      vk::BufferMemoryBarrier prefix_to_scatter_barrier {
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .buffer = draw.draw_prefix_buffer.buffer(),
        .offset = 0,
        .size = draw.draw_prefix_buffer.byte_size(),
      };
      _late_culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                                  vk::PipelineStageFlagBits::eComputeShader,
                                                  {}, {}, {prefix_to_scatter_barrier}, {});
    }

    uint32_t collect_scatter_push_constants[] {
      max_draws_count,
      draw.meshes_data_buffer_id,
      draw.draw_commands_buffer_id,
      draw.meshes_render_info_id,
      scene->_counters_buffer_id,
      draw.draw_counter_index,
      draw.draw_visibility_buffer_id,
      draw.draw_prefix_buffer_id,
    };
    _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _instances_collect_pipeline.pipeline());
    _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                   {_instances_collect_pipeline.layout()},
                                                   bindless_set_number,
                                                   culling_descriptor_sets,
                                                   {});
    _late_culling_command_unit->pushConstants(_instances_collect_pipeline.layout(), vk::ShaderStageFlagBits::eCompute,
                                              0, sizeof(collect_scatter_push_constants), collect_scatter_push_constants);

    _late_culling_command_unit->dispatch(calculate_work_groups_number(max_draws_count, culling_work_group_size), 1, 1);
  }

  // --------------------------------------------------------------------------
  // Culling statistics collecting
  // --------------------------------------------------------------------------

  if (is_render_option_enabled(_config.options, RenderOptions::EnableCullingStats)) {
    vk::BufferMemoryBarrier options_fill_barrier {
      .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
      .dstAccessMask = vk::AccessFlagBits::eTransferRead,
      .buffer = _culling_stat_buffer.buffer(),
      .offset = 0,
      .size = _culling_stat_buffer.byte_size(),
    };
    _late_culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                                vk::PipelineStageFlagBits::eTransfer,
                                                {}, {}, {options_fill_barrier}, {});
    bufcopy(_late_culling_command_unit, BufferRegion(_culling_stat_buffer), BufferRegion(_culling_stat_stage_buffer));
  }

  // --------------------------------------------------------------------------
  // Culling visualization
  // --------------------------------------------------------------------------

  if (is_render_option_enabled(_config.options, RenderOptions::EnableCullingVisualiztion)) {
    if (_save_culling_visualization || _clear_culling_visualization) {
      for (auto &[pipeline, draw] : scene->_draws) {
        _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute,
          _copy_visibility_states_pipeline.pipeline());

        _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                 {_copy_visibility_states_pipeline.layout()},
                                                 bindless_set_number,
                                                 culling_descriptor_sets,
                                                 {});

        uint32_t instances_number = static_cast<uint32_t>(draw.instances_data_buffer_data.size());
        uint32_t copy_visibility_push_contants[] {
          draw.instances_data_buffer_id,
          instances_number,
          scene->_occluded_instances_state_buffer_id,
          _save_culling_visualization ? 0u : 1u,
        };
        _late_culling_command_unit->pushConstants(_copy_visibility_states_pipeline.layout(),
                                                  vk::ShaderStageFlagBits::eCompute,
                                                  0, sizeof(copy_visibility_push_contants),
                                                  copy_visibility_push_contants);

        _late_culling_command_unit->dispatch(
          calculate_work_groups_number(instances_number, culling_work_group_size), 1, 1);

        vk::BufferMemoryBarrier visibility_states_barrier {
          .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
          .dstAccessMask = vk::AccessFlagBits::eIndirectCommandRead,
          .buffer = scene->_occluded_instances_state_buffer.buffer(),
          .offset = 0,
          .size = scene->_occluded_instances_state_buffer.byte_size(),
        };
        _late_culling_command_unit->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                                    vk::PipelineStageFlagBits::eDrawIndirect,
                                                    {}, {}, {visibility_states_barrier}, {});
      }
      _save_culling_visualization = false;
      _clear_culling_visualization = false;
    }
  }

  _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                       _timestamps_query_pool.get(),
                                       enum_cast(Timestamp::LateCullingEnd));

  _late_culling_command_unit.end();

  _late_culling_command_unit.add_wait_semaphore(_visible_models_rendering_semaphore.get(),
                                           vk::PipelineStageFlagBits::eComputeShader);
  // OcDrawAlways skips late render, which normally signals _models_render_finished_semaphore.
  // Lights wait on that semaphore — signal it from late cull instead.
  if (is_render_option_enabled(_config.options, RenderOptions::OcDrawAlways)) {
    _late_culling_command_unit.add_signal_semaphore(_models_render_finished_semaphore.get());
  } else {
    _late_culling_command_unit.add_signal_semaphore(_late_culling_semaphore.get());
  }

  vk::SubmitInfo culling_submit_info = _late_culling_command_unit.submit_info();
  _state->queue().submit(culling_submit_info);
}


void mr::RenderContext::update_bound_boxes_data()
{
  if (_render_bounds_state == RenderBoundsState::Disable) {
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
  if (_render_bounds_state == RenderBoundsState::Disable) {
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
    enum_cast<uint32_t>(_render_bounds_state),
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

    // TODO(dk6): Maybe made another funciton for it
    if (not is_late_pass) {
      update_bound_boxes_data();
    }

    auto trans_sem = is_late_pass ? _pre_model_layout_transition_semaphore_late.get()
                                  : _pre_model_layout_transition_semaphore.get();
    trans_cmd_unit.end();
    trans_cmd_unit.add_signal_semaphore(trans_sem);

    cmd_unit.add_wait_semaphore(trans_sem, vk::PipelineStageFlagBits::eDrawIndirect);

    auto culling_sem = is_late_pass ? _late_culling_semaphore.get() : _culling_semaphore.get();
    cmd_unit.add_wait_semaphore(culling_sem, vk::PipelineStageFlagBits::eDrawIndirect);

    auto gbufs_attachs = _gbuffers | std::views::transform([](const ColorAttachmentImage &gbuf) {
      return gbuf.attachment_info();
    }) | std::ranges::to<InplaceVector<vk::RenderingAttachmentInfoKHR, gbuffers_number>>();
    auto depth_attachment_info = _depthbuffer.attachment_info();
    gbufs_attachs[enum_cast(GBuffer::Position)].clearValue.color.uint32[3] = std::numeric_limits<uint32_t>::max();

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

    const auto models_start = is_late_pass ? Timestamp::ModelsLatePassStart : Timestamp::ModelsFirstPassStart;
    const auto models_end = is_late_pass ? Timestamp::ModelsLatePassEnd : Timestamp::ModelsFirstPassEnd;
    cmd_unit->resetQueryPool(_timestamps_query_pool.get(), enum_cast(models_start), 2);
    cmd_unit->writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe,
                             _timestamps_query_pool.get(),
                             enum_cast(models_start));

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

    render_models(scene, cmd_unit, is_late_pass);

    cmd_unit->endRendering();
    cmd_unit->writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe,
                             _timestamps_query_pool.get(),
                             enum_cast(models_end));
  }

  TracyVkCollect(_models_tracy_gpu_context, cmd_unit.command_buffer());

  auto finish_semaphore =
    is_render_option_enabled(_config.options, RenderOptions::DisableOcclusionCulling) || is_late_pass
      ? _models_render_finished_semaphore.get()
      : _visible_models_rendering_semaphore.get();
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

  _depthbuffer.switch_layout(_late_culling_command_unit, vk::ImageLayout::eShaderReadOnlyOptimal);

  _late_culling_command_unit->bindPipeline(vk::PipelineBindPoint::eCompute, _depth_pyramid_pipeline.pipeline());
  _late_culling_command_unit->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                           {_depth_pyramid_pipeline.layout()},
                                           bindless_set_number,
                                           {_bindless_set.set()},
                                           {});

  auto dst_size = _depth_pyramid_extent;
  auto src_size = _extent;
  auto real_src_size = _depthbuffer.extent();
  for (uint32_t i = 0; dst_size.width > 0 && dst_size.height > 0; i++) {
    // just for pipeline barrier
    _depth_pyramid.switch_layout(_late_culling_command_unit, vk::ImageLayout::eGeneral, i, 1, true);
    // TODO(dk6): added switch layout for read mip

    uint32_t depth_pyramid_push_contants[] {
      i == 0 ? _depth_image_attacment_id : _depth_pyramid_mips[i - 1].descriptor_sampled_image_id, // src image
      _depth_pyramid_mips[i].descriptor_storage_image_id, // dst image
      dst_size.width,
      dst_size.height,

      // TODO(dk6): use already calcaulted coef from resize
      src_size.width,
      src_size.height,

      real_src_size.width,
      real_src_size.height,
    };

    _late_culling_command_unit->pushConstants({_depth_pyramid_pipeline.layout()}, vk::ShaderStageFlagBits::eCompute,
                                              0, sizeof(depth_pyramid_push_contants), depth_pyramid_push_contants);

    uint32_t work_group_width = calculate_work_groups_number(dst_size.width, culling_work_group_size);
    uint32_t work_group_height = calculate_work_groups_number(dst_size.height, culling_work_group_size);
    _late_culling_command_unit->dispatch(work_group_width, work_group_height, 1);

    dst_size.width /= 2;
    dst_size.height /= 2;
    src_size.width /= 2;
    src_size.height /= 2;
    real_src_size.width /= 2;
    real_src_size.height /= 2;
  }

  _late_culling_command_unit->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader,
                                       _timestamps_query_pool.get(),
                                       enum_cast(Timestamp::BuildDepthPyramidEnd));
}

void mr::RenderContext::build_tiled_msoc_buffer()
{
  // MSOC currently aliases depth pyramid mip L via _msoc_tiled_depth_resource.
  // Future: allocate StorageImage, run depth_build_tiled_buffer shader here,
  // register as ShaderImageResource, assign _tiled_buffer_image_id.
}

void mr::RenderContext::sync_msoc_tiled_buffer_scale()
{
  if (!uses_msoc_pipeline(_config.oc_type)) {
    return;
  }

  if (_config.oc_type == OcclusionCullingType::MsocAdaptiveTileSize) {
    return;
  }

  const uint32_t mip = msoc_tile_mip_level(_config.msoc_tile_size.width);
  _tiled_buffer_scale[0] = _depth_pyramid_mips_scale_coefs_buffer_data[2 * mip + 0];
  _tiled_buffer_scale[1] = _depth_pyramid_mips_scale_coefs_buffer_data[2 * mip + 1];
}

void mr::RenderContext::resize(const mr::Extent &extent)
{
  _extent = extent;
  // TODO(dk6): number of mips can be changed too
  _depth_pyramid_extent = calculate_depth_pyramid_extent(_extent);

  // Calculate scale coefs for depth pyramid
  std::array<float, depth_pyramid_max_levels * 2> scales;
  std::ranges::fill(scales, 0.0f);

  auto real = _depth_pyramid.extent();
  auto ext = _depth_pyramid_extent;

  uint32_t i = 0;
  while (ext.width != 0 && ext.height != 0) {
    _depth_pyramid_mips_scale_coefs_buffer_data[i + 0] = static_cast<float>(ext.width) / real.width;
    _depth_pyramid_mips_scale_coefs_buffer_data[i + 1] = static_cast<float>(ext.height) / real.height;

    ext.width /= 2;
    ext.height /= 2;
    real.width /= 2;
    real.height /= 2;
    i += 2;
  }

  sync_msoc_tiled_buffer_scale();
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

  calculate_prev_stat(scene);

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

  render_geometry(scene, false);

  if (not is_render_option_enabled(_config.options, RenderOptions::DisableOcclusionCulling)) {
    // ------------------------------------------------
    // Late frusum culling and occlusion culling
    // ------------------------------------------------

    late_culling_geometry(scene);

    // OcDrawAlways: first pass already drew all in-frustum (like DisableOcclusionCulling).
    // Late OC still runs to fill WAS_OCCLUDED bits for debug dump; skip late draw.
    if (not is_render_option_enabled(_config.options, RenderOptions::OcDrawAlways)) {
      // ------------------------------------------------
      // Rendering previously invisible models
      // ------------------------------------------------

      render_geometry(scene, true);
    }
  }

  // --------------------------------------------------------------------------
  // Lights shading pass
  // --------------------------------------------------------------------------

  render_lights(scene, presenter);

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

  if (is_render_option_enabled(_config.options, RenderOptions::CollectPosInstanceId)) {
    _lights_command_unit.add_signal_semaphore(_gbuffers_data_copy_ready_semaphore.get());
  }

  vk::SubmitInfo pre_light_layout_transition_submit_info =
    _pre_light_layout_transition_command_unit.submit_info();
  _state->queue().submit(pre_light_layout_transition_submit_info);

  vk::SubmitInfo light_submit_info = _lights_command_unit.submit_info();
  _state->queue().submit(light_submit_info, _image_fence.get());

  // Must run before present(): RenderDoc treats vkQueuePresentKHR as the frame
  // boundary. Signaling _gbuffers_data_copy_ready before present and waiting
  // after it splits a binary semaphore across frames → capture/replay hang.
  if (is_render_option_enabled(_config.options, RenderOptions::CollectPosInstanceId)) {
    auto &pos_gbuf = _gbuffers[enum_cast(GBuffer::Position)];
    _position_instance_copy_cmd_unit.begin();
    pos_gbuf.read_to_host_buffer(_position_instance_copy_cmd_unit, _position_instance_id_stage_buffer);
    _position_instance_copy_cmd_unit.end();
    _position_instance_copy_cmd_unit.add_wait_semaphore(_gbuffers_data_copy_ready_semaphore.get(),
                                                         vk::PipelineStageFlagBits::eTransfer);
    vk::SubmitInfo copy_gbuf_submit_info = _position_instance_copy_cmd_unit.submit_info();
    _state->queue().submit(copy_gbuf_submit_info, _position_instance_copy_fence.get());
  }

  presenter.present();

  if (_dump_scene_objects.exchange(false)) {
    dump_scene_objects_visibility(scene);
  }

  FrameMarkEnd(frame_name);

  calculate_stat(scene, render_start_time, ClockT::now());
}

void mr::RenderContext::calculate_stat(SceneHandle scene,
                                       ClockT::time_point render_start_time,
                                       ClockT::time_point render_finish_time) noexcept
{
  auto get_ms = [](ClockT::duration time) -> double {
    using namespace std::chrono;
    return duration_cast<duration<double, std::milli>>(time).count();
  };
  _render_stat.render_cpu_time_ms = get_ms(render_finish_time - render_start_time);
  _render_stat.cpu_time_ms = get_ms(render_start_time - _prev_start_time);
  _render_stat.cpu_fps = 1000 / _render_stat.cpu_time_ms;

  _prev_start_time = render_start_time;

  _state->device().waitForFences(_image_fence.get(), VK_TRUE, UINT64_MAX);

  using QueryRes = std::pair<uint64_t, uint32_t>;
  vk::QueryResultFlags query_res_flags = vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability;

  std::array<QueryRes, timestamps_number> timestamps;
  _state->device().getQueryPoolResults(_timestamps_query_pool.get(),
                                       0, timestamps_number,
                                       sizeof(timestamps[0]) * timestamps_number,
                                       timestamps.data(),
                                       sizeof(timestamps[0]), // stride
                                       query_res_flags);

  const auto timestamp_delta_ms = [&](Timestamp start, Timestamp end) -> double {
    const auto &start_ts = timestamps[enum_cast(start)];
    const auto &end_ts = timestamps[enum_cast(end)];
    if (start_ts.second == 0 || end_ts.second == 0) {
      return 0.0;
    }
    return static_cast<double>(end_ts.first - start_ts.first) * _timestamp_to_ms;
  };

  _render_stat.culling_gpu_time_ms = timestamp_delta_ms(Timestamp::CullingStart, Timestamp::CullingEnd);
  _render_stat.models_first_pass_gpu_time_ms =
    timestamp_delta_ms(Timestamp::ModelsFirstPassStart, Timestamp::ModelsFirstPassEnd);
  _render_stat.models_late_pass_gpu_time_ms =
    timestamp_delta_ms(Timestamp::ModelsLatePassStart, Timestamp::ModelsLatePassEnd);
  _render_stat.models_gpu_time_ms =
    _render_stat.models_first_pass_gpu_time_ms + _render_stat.models_late_pass_gpu_time_ms;
  _render_stat.build_depth_pyramid_gpu_time_ms =
    timestamp_delta_ms(Timestamp::BuildDepthPyramidStart, Timestamp::BuildDepthPyramidEnd);
  _render_stat.late_culling_gpu_time_ms =
    timestamp_delta_ms(Timestamp::LateCullingStart, Timestamp::LateCullingEnd);
  _render_stat.msoc_tile_prep_gpu_time_ms =
    timestamp_delta_ms(Timestamp::MsocTilePrepStart, Timestamp::MsocTileScanStart);
  _render_stat.msoc_tile_scan_gpu_time_ms =
    timestamp_delta_ms(Timestamp::MsocTileScanStart, Timestamp::MsocTileFinalizeStart);
  _render_stat.msoc_tile_finalize_gpu_time_ms =
    timestamp_delta_ms(Timestamp::MsocTileFinalizeStart, Timestamp::MsocTileTestStart);
  _render_stat.msoc_tile_test_gpu_time_ms =
    timestamp_delta_ms(Timestamp::MsocTileTestStart, Timestamp::MsocTileApplyStart);
  _render_stat.msoc_tile_apply_gpu_time_ms =
    timestamp_delta_ms(Timestamp::MsocTileApplyStart, Timestamp::MsocTileApplyEnd);
  _render_stat.occlusion_culling_gpu_time_ms =
    _render_stat.build_depth_pyramid_gpu_time_ms + _render_stat.late_culling_gpu_time_ms;
  _render_stat.shading_gpu_time_ms = timestamp_delta_ms(Timestamp::ShadingStart, Timestamp::ShadingEnd);
  _render_stat.render_gpu_time_ms = timestamp_delta_ms(Timestamp::CullingStart, Timestamp::ShadingEnd);
  _render_stat.gpu_measured_stages_ms = _render_stat.culling_gpu_time_ms +
                                        _render_stat.models_gpu_time_ms +
                                        _render_stat.occlusion_culling_gpu_time_ms +
                                        _render_stat.shading_gpu_time_ms;

  _render_stat.gpu_time_ms = (timestamps[enum_cast(Timestamp::CullingStart)].first - _prev_first_timestamp)
                             * _timestamp_to_ms;
  _prev_first_timestamp = timestamps[enum_cast(Timestamp::CullingStart)].first;
  _render_stat.gpu_fps = 1000 / _render_stat.gpu_time_ms;

  _render_stat.triangles_number = scene->_triangles_number.load();
  _render_stat.vertexes_number = scene->_vertexes_number.load();
  _render_stat.frame_number = _frame_number++;

  if (is_render_option_enabled(_config.options, RenderOptions::EnableCullingStats)) {
    auto data = _culling_stat_stage_buffer.copy();
    const CullingStats *stat = reinterpret_cast<const CullingStats *>(data.data());
    _render_stat.total_objects_number = stat->total_objects_number;
    _render_stat.msoc_tiles_number = stat->msoc_tiles_number;
    _render_stat.outside_frustum_objects_number = stat->outside_frustum_objects_number;
    _render_stat.occluded_objects_number = stat->occluded_objects_number;
    _render_stat.visible_objects_number =
      stat->total_objects_number - stat->occluded_objects_number - stat->outside_frustum_objects_number;
  }
}

void mr::RenderContext::calculate_prev_stat(SceneHandle scene) noexcept
{
  if (is_render_option_enabled(_config.options, RenderOptions::EnableCullingStats) &&
      is_render_option_enabled(_config.options, RenderOptions::CollectPosInstanceId)) {
    _state->device().waitForFences({_position_instance_copy_fence.get()},
                                   vk::True, std::numeric_limits<uint64_t>::max());
    _state->device().resetFences({_position_instance_copy_fence.get()});

    auto &pos_gbuf = _gbuffers[enum_cast(GBuffer::Position)];
    auto [w, h, _z] = pos_gbuf.extent();
    assert(_position_instance_id_stage_buffer.byte_size() == format_byte_size(pos_gbuf.format()) * w * h);
    assert(format_byte_size(pos_gbuf.format()) == sizeof(float) * 4);

    // Copy from mapped host visible memory about 4 ms because we use readback
    _position_instance_id_data = _position_instance_id_stage_buffer.copy();
    const float *data = reinterpret_cast<const float *>(_position_instance_id_data.data());

    // Iteration over 1920x1080 gbuf takes about 7 ms. It can be used ONLY for debug
    // calculate really visible objects
    boost::unordered_set<uint32_t> visible_objects; // maybe use boost::container::flat_set
    uint32_t pixels_nums = w * h;
    for (uint32_t i = 0; i < pixels_nums; i++) {
      uint32_t index = i * 4 + 3;
      uint32_t id = std::bit_cast<uint32_t>(data[index]);
      if (id != std::numeric_limits<uint32_t>::max()) {
        visible_objects.insert(id);
      }
    }
    _render_stat.really_visible_objects_number = static_cast<uint32_t>(visible_objects.size());

    uint32_t in_frustum_objects = _render_stat.total_objects_number - _render_stat.outside_frustum_objects_number;
    _render_stat.not_occluded_in_frustum_objects = in_frustum_objects - _render_stat.occluded_objects_number;
    _render_stat.occlusion_culling_accuracy = _render_stat.not_occluded_in_frustum_objects != 0
      ? (_render_stat.really_visible_objects_number / double(_render_stat.not_occluded_in_frustum_objects))
      : (_render_stat.really_visible_objects_number == 0 ? 1.0f : (0.5f / _render_stat.really_visible_objects_number));
  }
  _prev_render_stat = _render_stat;
}

std::optional<mr::Vec4f> mr::RenderContext::get_position_id_pixel(uint32_t x, uint32_t y) const noexcept
{
  if (_position_instance_id_data.empty()) {
    return std::nullopt;
  }

  auto &pos_gbuf = _gbuffers[enum_cast(GBuffer::Position)];
  auto [w, h, _z] = pos_gbuf.extent();
  if (x >= w || y >= h) {
    return std::nullopt;
  }

  const float *data = reinterpret_cast<const float *>(_position_instance_id_data.data());
  uint32_t idx = (y * w + x) * 4;
  return Vec4f(data[idx], data[idx + 1], data[idx + 2], data[idx + 3]);
}

bool mr::RenderContext::save_gbuffer_to_file(const std::filesystem::path &filename)
{
  if (not is_render_option_enabled(_config.options, RenderOptions::CollectPosInstanceId)) {
    MR_ERROR("save_gbuffer_to_file requires CollectPosInstanceId (--read-gbuf)");
    return false;
  }

  _state->device().waitForFences({_position_instance_copy_fence.get()},
                                 vk::True, std::numeric_limits<uint64_t>::max());

  auto &pos_gbuf = _gbuffers[enum_cast(GBuffer::Position)];
  auto [w, h, _z] = pos_gbuf.extent();
  const size_t expected_bytes = format_byte_size(pos_gbuf.format()) * static_cast<size_t>(w) * h;
  assert(_position_instance_id_stage_buffer.byte_size() == expected_bytes);
  assert(format_byte_size(pos_gbuf.format()) == sizeof(float) * 4);

  auto pixels = _position_instance_id_stage_buffer.copy();
  assert(pixels.size() == expected_bytes);

  std::ofstream out(filename, std::ios::binary);
  if (not out) {
    MR_ERROR("save_gbuffer_to_file: failed to open '{}'", filename.string());
    return false;
  }

  const uint32_t width = w;
  const uint32_t height = h;
  out.write(reinterpret_cast<const char *>(&width), sizeof(width));
  out.write(reinterpret_cast<const char *>(&height), sizeof(height));
  out.write(reinterpret_cast<const char *>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
  if (not out) {
    MR_ERROR("save_gbuffer_to_file: failed to write '{}'", filename.string());
    return false;
  }
  return true;
}

bool mr::RenderContext::dump_scene_objects_visibility(SceneHandle scene,
                                                      const std::filesystem::path &filename)
{
  if (not is_render_option_enabled(_config.options, RenderOptions::CollectPosInstanceId)) {
    MR_ERROR("dump_scene_objects_visibility requires CollectPosInstanceId (--read-gbuf)");
    return false;
  }
  ASSERT(scene != nullptr);

  constexpr uint32_t instance_in_frustum_bit = 0;
  constexpr uint32_t instance_was_occluded_bit = 1;
  constexpr uint32_t invalid_id = std::numeric_limits<uint32_t>::max();

  _state->device().waitForFences({_position_instance_copy_fence.get()},
                                 vk::True, std::numeric_limits<uint64_t>::max());

  _position_instance_id_data = _position_instance_id_stage_buffer.copy();
  const float *gbuf = reinterpret_cast<const float *>(_position_instance_id_data.data());
  auto &pos_gbuf = _gbuffers[enum_cast(GBuffer::Position)];
  auto [w, h, _z] = pos_gbuf.extent();
  boost::unordered_set<uint32_t> really_visible;
  const uint32_t pixels = w * h;
  for (uint32_t i = 0; i < pixels; i++) {
    uint32_t id = std::bit_cast<uint32_t>(gbuf[i * 4 + 3]);
    if (id != invalid_id) {
      really_visible.insert(id);
    }
  }

  struct Row {
    uint32_t id;
    uint32_t visible;
    uint32_t really_visible;
  };
  std::vector<Row> rows;
  rows.reserve(really_visible.size() + 1024);
  std::vector<uint32_t> false_cull_ids;

  for (auto &[pipeline, draw] : scene->_draws) {
    const uint32_t instances_number = static_cast<uint32_t>(draw.instances_data_buffer_data.size());
    if (instances_number == 0) {
      continue;
    }
    const size_t bytes = sizeof(Scene::MeshInstanceCullingData) * instances_number;
    HostBuffer stage(*_state, bytes, vk::BufferUsageFlagBits::eTransferDst);

    _transfer_command_unit.begin();
    bufcopy(_transfer_command_unit,
            BufferRegion{draw.instances_data_buffer, 0, bytes},
            BufferRegion{stage, 0, bytes});
    _transfer_command_unit.end();
    UniqueFenceGuard(_state->device(), _transfer_command_unit.submit(*_state));

    auto raw = stage.copy();
    const auto *instances = reinterpret_cast<const Scene::MeshInstanceCullingData *>(raw.data());
    for (uint32_t i = 0; i < instances_number; i++) {
      const uint32_t id = instances[i].transform_index;
      const uint32_t bits = instances[i].visible_last_frame;
      const bool in_frustum = (bits & (1u << instance_in_frustum_bit)) != 0;
      const bool was_occluded = (bits & (1u << instance_was_occluded_bit)) != 0;
      const uint32_t visible = (in_frustum && not was_occluded) ? 1u : 0u;
      const uint32_t on_screen = really_visible.contains(id) ? 1u : 0u;
      rows.push_back(Row{id, visible, on_screen});
      if (visible == 0 && on_screen == 1) {
        false_cull_ids.push_back(id);
      }
    }
  }

  std::println(std::cerr, "false culls: {}", false_cull_ids.size());
  for (uint32_t id : false_cull_ids) {
    std::println(std::cerr, "{}", id);
  }

  std::ofstream out(filename);
  if (not out) {
    MR_ERROR("dump_scene_objects_visibility: failed to open '{}'", filename.string());
    return false;
  }
  std::println(out, "id,visible,really_visible");
  for (const auto &row : rows) {
    std::println(out, "{},{},{}", row.id, row.visible, row.really_visible);
  }
  MR_INFO("Wrote {} rows to {}", rows.size(), filename.string());
  return true;
}

void mr::RenderStat::write_to_json(std::ostream &out) const noexcept
{
  const double render_gpu_time_s = std::max(render_gpu_time_ms / 1000.0, 1e-9);
  double triangles_per_second = triangles_number / render_gpu_time_s;
  std::println(out, "{{");
  std::println(out, "  \"frame_number\": {},", frame_number);
  std::println(out, "  \"cpu_fps\": {:.2f},", cpu_fps);
  std::println(out, "  \"cpu_time_ms\": {:.2f},", cpu_time_ms);
  std::println(out, "  \"gpu_fps\": {:.2f},", gpu_fps);
  std::println(out, "  \"gpu_time_ms\": {:.2f},", gpu_time_ms);
  std::println(out, "  \"cpu_rendering_time_ms\": {:.2f},", render_cpu_time_ms);
  std::println(out, "  \"culling_gpu_time_ms\": {:.3f},", culling_gpu_time_ms);
  std::println(out, "  \"build_depth_pyramid_gpu_time_ms\": {:.3f},", build_depth_pyramid_gpu_time_ms);
  std::println(out, "  \"late_culling_gpu_time_ms\": {:.3f},", late_culling_gpu_time_ms);
  std::println(out, "  \"msoc_tile_prep_gpu_time_ms\": {:.3f},", msoc_tile_prep_gpu_time_ms);
  std::println(out, "  \"msoc_tile_scan_gpu_time_ms\": {:.3f},", msoc_tile_scan_gpu_time_ms);
  std::println(out, "  \"msoc_tile_finalize_gpu_time_ms\": {:.3f},", msoc_tile_finalize_gpu_time_ms);
  std::println(out, "  \"msoc_tile_test_gpu_time_ms\": {:.3f},", msoc_tile_test_gpu_time_ms);
  std::println(out, "  \"msoc_tile_apply_gpu_time_ms\": {:.3f},", msoc_tile_apply_gpu_time_ms);
  std::println(out, "  \"occlusion_culling_gpu_time_ms\": {:.3f},", occlusion_culling_gpu_time_ms);
  std::println(out, "  \"gpu_rendering_time_ms\": {:.2f},", render_gpu_time_ms);
  std::println(out, "  \"gpu_measured_stages_ms\": {:.2f},", gpu_measured_stages_ms);
  std::println(out, "  \"gpu_models_first_pass_time_ms\": {:.2f},", models_first_pass_gpu_time_ms);
  std::println(out, "  \"gpu_models_late_pass_time_ms\": {:.2f},", models_late_pass_gpu_time_ms);
  std::println(out, "  \"gpu_models_time_ms\": {:.2f},", models_gpu_time_ms);
  std::println(out, "  \"gpu_shading_time_ms\": {:.2f},", shading_gpu_time_ms);
  std::println(out, "  \"triangles_per_second\": {:.3f},", triangles_per_second);
  std::println(out, "  \"triangles_per_second_millions\": {:.3f},", triangles_per_second * 1e-6);
  std::println(out, "  \"triangles_number\": {},", triangles_number);
  std::println(out, "  \"vertexes_number\": {},", vertexes_number);

  // These all value can be 0
  std::println(out, "  \"total_objects_number\": {},", total_objects_number);
  std::println(out, "  \"msoc_tiles_number\": {},", msoc_tiles_number);
  std::println(out, "  \"outside_frustum_objects_number\": {},", outside_frustum_objects_number);
  std::println(out, "  \"visible_objects_number\": {},", visible_objects_number);
  std::println(out, "  \"occluded_objects_number\": {},", occluded_objects_number);
  std::println(out, "  \"really_visible_objects_number\": {},", really_visible_objects_number);
  std::println(out, "  \"not_occluded_in_frustum_objects\": {},", not_occluded_in_frustum_objects);
  std::println(out, "  \"occlusion_culling_accuracy\": {}", occlusion_culling_accuracy);

  std::println(out, "}}");
}
