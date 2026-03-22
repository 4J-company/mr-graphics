#ifndef __MR_RENDER_CONTEXT_HPP_
#define __MR_RENDER_CONTEXT_HPP_

#include "pch.hpp"

#include "vulkan_state.hpp"
#include "render_context_options.hpp"

#include "camera/camera.hpp"
#include "lights/lights.hpp"
#include "model/model.hpp"
#include "scene/scene.hpp"

#include "resources/images/image.hpp"
#include "resources/resources.hpp"
#include "resources/command_unit/command_unit.hpp"

#include "window.hpp"
#include "file_writer.hpp"
#include "dummy_presenter.hpp"

#include <VkBootstrap.h>
#include <vulkan/vulkan_core.h>

namespace mr {
inline namespace graphics {
  class Window;

  struct RenderStat {
    uint32_t frame_number = 0;

    double culling_gpu_time_ms = 0;
    double late_culling_gpu_time_ms = 0;
    double build_depth_pyramid_gpu_time_ms = 0;
    double render_gpu_time_ms = 0;
    double models_gpu_time_ms = 0;
    double shading_gpu_time_ms = 0;

    double gpu_time_ms = 0;
    double gpu_fps = 0;

    double render_cpu_time_ms = 0; // time of rendering

    double cpu_time_ms = 0; // time between calls of render
    double cpu_fps = 0;

    uint64_t vertexes_number = 0;
    uint64_t triangles_number = 0;

    // This field fills if EnableCullingStat option is enabled
    uint32_t total_objects_number = 0;
    uint32_t outside_frustum_objects_number = 0;
    uint32_t occluded_objects_number = 0;
    uint32_t visible_objects_number = 0; // extra information
    uint32_t really_visible_objects_number = 0;
    uint32_t not_occluded_in_frustum_objects = 0; // extra information
    double occlusion_culling_accuracy = 0;

    void write_to_json(std::ostream &out) const noexcept;
  };

  class RenderContext {
  public:
    static inline constexpr int gbuffers_number = 6;
    static inline constexpr int max_images_number = 8; // max teoretical swapchain images number

    // GBuffers names
    enum struct GBuffer : uint32_t {
      Position = 0,
      NormalIsShade = 1,
      OMR = 2, // Occlusion Metallic Roughness
      Emissive = 3,
      Occlusion = 4,
      ColorTrans = 5
    };

    enum RenderBoundsState : uint32_t {
      Disable,
      BoundBoxes,
      BoundRectangles,
      // BoundSpheres // TODO
      StatesNumber,
    };

    // Bindings numbers in bindless descriptor set
    constexpr static inline uint32_t textures_binding = 0;
    constexpr static inline uint32_t uniform_buffer_binding = 1;
    constexpr static inline uint32_t storage_buffer_binding = 2;
    constexpr static inline uint32_t storage_images_binding = 3;
    constexpr static inline uint32_t bindless_set_number = 0;

    constexpr static inline uint32_t default_vertex_number = 10'000'000;
    constexpr static inline uint32_t default_index_number = default_vertex_number * 2;

    constexpr static inline uint32_t culling_work_group_size = 32;

    // It is enough for 64k x 64k screen size
    constexpr static inline uint32_t depth_pyramid_max_levels = 16;

  private:
    // Timestamps
    enum struct Timestamp : uint32_t {
      CullingStart,
      CullingEnd,
      ModelsStart,
      ModelsEnd,
      BuildDepthPyramidStart,
      BuildDepthPyramidEnd,
      LateCullingStart,
      LateCullingEnd,
      ShadingStart,
      ShadingEnd,
      TimestampsNumber,
    };
    constexpr static inline uint32_t timestamps_number = enum_cast(Timestamp::TimestampsNumber);

    using ClockT = std::chrono::steady_clock;

    struct BoundBoxRenderData {
      uint32_t transforms_buffer_id;
      uint32_t transform_index;
      uint32_t bound_boxes_buffer_id; // TODO: move to push contants
      uint32_t bound_spheres_buffer_id; // TODO: move to push contants
      uint32_t bound_box_index;
    };

    struct DepthPyramidMip {
      ShaderPyramidImageLevelResource storage_image_resource;
      ShaderPyramidImageLevelResource sampled_image_resource;
      uint32_t descriptor_storage_image_id;
      uint32_t descriptor_sampled_image_id;
    };

    struct CullingStats {
      uint32_t total_objects_number = 0;
      uint32_t outside_frustum_objects_number = 0;
      uint32_t occluded_objects_number = 0;
    };

  private:
    std::shared_ptr<VulkanState> _state;
    Extent _extent;
    RenderOptions _render_options;

    vk::UniqueQueryPool _timestamps_query_pool {};
    RenderStat _render_stat, _prev_render_stat;
    ClockT::time_point _prev_start_time {};
    uint64_t _prev_first_timestamp = 0;
    double _timestamp_to_ms = 0;
    uint32_t _frame_number = 0;

    TracyVkCtx _models_tracy_gpu_context {};
    TracyVkCtx _lights_tracy_gpu_context {};

    CommandUnit _models_command_unit;
    CommandUnit _late_models_command_unit;
    CommandUnit _lights_command_unit;
    // RenderContext doesn't use transfer command unit, only gives it for buffers
    // Writting commands to it doesn't affect RenderContext internal state
    mutable CommandUnit _transfer_command_unit;

    // TODO(dk6): use Framedata instead
    InplaceVector<ColorAttachmentImage, gbuffers_number> _gbuffers;
    DepthImage _depthbuffer;
    ShaderImageResource _depthbuffer_resource;

    // semaphores for waiting swapchain image is ready before light pass
    InplaceVector<vk::UniqueSemaphore, max_images_number> _image_available_semaphore;
    // semaphores for waiting frame is ready before presentin
    InplaceVector<vk::UniqueSemaphore, max_images_number> _render_finished_semaphore;

    vk::UniqueSemaphore _pre_model_layout_transition_semaphore;
    CommandUnit _pre_model_layout_transition_command_unit;
    CommandUnit _pre_model_layout_transition_command_unit_late;
    vk::UniqueSemaphore _pre_model_layout_transition_semaphore_late;

    vk::UniqueSemaphore _pre_light_layout_transition_semaphore;
    CommandUnit _pre_light_layout_transition_command_unit;

    // semaphore for sync opaque models rendering and light shading
    vk::UniqueSemaphore _models_render_finished_semaphore;
    vk::UniqueFence _image_fence; // fence for swapchain image?

    LightsRenderData _lights_render_data;

    // TODO(dk6): Maybe move to class scene
    // Bindless rednering data
    DescriptorAllocator _default_descriptor_allocator;
    BindlessDescriptorSetLayoutHandle _bindless_set_layout;
    DescriptorSetLayoutHandle _converted_bindless_set_layout;
    BindlessDescriptorSet _bindless_set;

    DeviceHeapAllocator _vertex_buffers_heap;
    VertexVectorBuffer _positions_vertex_buffer;
    VertexVectorBuffer _attributes_vertex_buffer;
    IndexHeapBuffer _index_buffer;

    CommandUnit _culling_command_unit;
    CommandUnit _late_culling_command_unit;
    vk::UniqueSemaphore _culling_semaphore;
    vk::UniqueSemaphore _visible_models_rendering_semaphore;
    vk::UniqueSemaphore _late_culling_semaphore;
    ShaderHandle _instances_culling_shader;
    ComputePipeline _instances_culling_pipeline;
    ShaderHandle _instances_collect_shader;
    ComputePipeline _instances_collect_pipeline;
    ShaderHandle _late_instances_culling_shader;
    ComputePipeline _late_instances_culling_pipeline;

    // --- Collecting world coordinates and instances id ---
    vk::UniqueSemaphore _gbuffers_data_copy_ready_semaphore;
    CommandUnit _position_instance_copy_cmd_unit;
    vk::UniqueFence _position_instance_copy_fence;
    HostBuffer _position_instance_id_stage_buffer;

    // --- This used if EnableCullingStats option is enabled ---
    StorageBuffer _culling_stat_buffer;
    uint32_t _culling_stat_buffer_id = BindlessDescriptorSet::invalid_id;
    HostBuffer _culling_stat_stage_buffer;

    // --- This used if EnableCullingVisualization option is enabled ---
    std::atomic_bool _save_culling_visualization = false;
    std::atomic_bool _clear_culling_visualization = false;
    ShaderHandle _copy_visibility_states_shader;
    ComputePipeline _copy_visibility_states_pipeline;
    ShaderHandle _copy_on_screen_shader;
    ComputePipeline _copy_on_screen_pipeline;
    Sampler _read_from_gbuf_sampler;
    ShaderImageResource _read_from_gbuf_resource;
    uint32_t _sampled_gbuffer_id;
    ShaderHandle _clear_on_screen_state_shader;
    ComputePipeline _clear_on_screen_state_pipeline;

    Extent _depth_pyramid_extent;
    PyramidImage _depth_pyramid;
    ShaderImageResource _depth_pyramid_resource;
    StorageBuffer _depth_pyramid_mips_scale_coefs_buffer;
    uint32_t _depth_pyramid_mips_scale_coefs_buffer_id = BindlessDescriptorSet::invalid_id;
    std::array<float, depth_pyramid_max_levels * 2> _depth_pyramid_mips_scale_coefs_buffer_data;
    InplaceVector<DepthPyramidMip, depth_pyramid_max_levels> _depth_pyramid_mips;
    uint32_t _depth_pyramid_image_id = BindlessDescriptorSet::invalid_id;
    Sampler _depth_sampler;
    uint32_t _depth_image_attacment_id = BindlessDescriptorSet::invalid_id;
    ComputePipeline _depth_pyramid_pipeline;
    ShaderHandle _depth_pyramid_shader;

    // TODO(dk6): rework it to MarkerSystem
    ShaderHandle _bound_boxes_draw_shader;
    GraphicsPipeline _bound_boxes_draw_pipeline;
    StorageBuffer _bound_boxes_buffer;
    uint32_t _bound_boxes_buffer_id = -1;
    std::vector<BoundBoxRenderData> _bound_boxes_data;
    std::atomic_bool _bound_boxes_data_dirty = false;
    RenderBoundsState _render_bounds_state = RenderBoundsState::Disable;

  public:
    RenderContext(RenderContext &&other) noexcept = default;
    RenderContext & operator=(RenderContext &&other) noexcept = default;

    RenderContext(const RenderContext &other) noexcept = delete;
    RenderContext & operator=(const RenderContext &other) noexcept = delete;

    // TODO(dk6): change pointer to reference
    RenderContext(VulkanGlobalState *global_state, Extent extent, RenderOptions options = RenderOptions::None);

    ~RenderContext();

    void resize(const mr::Extent &extent);

    void render(const SceneHandle scene, Presenter &presenter);

    // ===== Getters =====
    const LightsRenderData & lights_render_data() const noexcept { return _lights_render_data; }
    const VulkanState & vulkan_state() const noexcept { return *_state; }
    const Extent & extent() const noexcept { return _extent; }
    const RenderStat & stat() const noexcept { return _render_stat; }
    const RenderStat & prev_stat() const noexcept { return _prev_render_stat; }
    RenderOptions options() const noexcept { return _render_options; }
    CommandUnit & transfer_command_unit() const noexcept { return _transfer_command_unit; }

    void render_bounds_state(RenderBoundsState state) noexcept { _render_bounds_state = state; }
    RenderBoundsState render_bounds_state() const noexcept { return _render_bounds_state; }

    // Works only if EnableCullingVisualiztion option is enabled
    void save_visibility() noexcept { _save_culling_visualization = true; }
    void clear_visibility() noexcept { _clear_culling_visualization = true; }

    IndexHeapBuffer & index_buffer() noexcept { return _index_buffer; }
    VertexBuffersArray add_vertex_buffers(CommandUnit &command_unit, std::span<const std::span<const std::byte>> vbufs_data) noexcept;
    void delete_vertex_buffers(std::span<const VertexBufferDescription> vbufs) noexcept;

    // ===== Resources creation =====
    WindowHandle create_window() const noexcept;
    WindowHandle create_window(const mr::Extent &extent) const noexcept;
    FileWriterHandle create_file_writer() const noexcept;
    FileWriterHandle create_file_writer(const mr::Extent &extent) const noexcept;
    DummyPresenterHandle create_dummy_presenter(const mr::Extent &extent) const noexcept;

    SceneHandle create_scene() noexcept;

    // ===== Bindless rendering =====
    DescriptorSetLayoutHandle bindless_set_layout() const noexcept { return _bindless_set_layout; }
    BindlessDescriptorSet & bindless_set() noexcept { return _bindless_set; }
    const BindlessDescriptorSet & bindless_set() const noexcept { return _bindless_set; }

    const DescriptorAllocator & desciptor_allocator() const noexcept { return _default_descriptor_allocator; }

    void draw_bound_box(uint32_t transforms_buffer_id, uint32_t transform_index,
                        uint32_t bound_boxes_buffer_id, uint32_t bound_spheres_buffer_id,
                        uint32_t bound_box_index) noexcept;

  private:
    void init_lights_render_data();
    void init_bindless_rendering();
    void init_profiling();
    void init_culling();
    void init_bound_box_rendering();

    // TODO(dk6): move bound boxes rendering in separate function instead this flag
    void render_geometry(const SceneHandle scene, bool is_late_pass);
    void culling_geometry(const SceneHandle scene);
    void late_culling_geometry(const SceneHandle scene);
    void build_depth_pyramid();
    void render_bound_boxes(const SceneHandle scene);
    void render_models(const SceneHandle scene, CommandUnit &cmd_unit);
    void render_lights(const SceneHandle scene, Presenter &presenter);

    void update_bound_boxes_data();
    void update_camera_buffer(UniformBuffer &uniform_buffer);

    void calculate_stat(SceneHandle scene,
                        ClockT::time_point render_start_time,
                        ClockT::time_point render_finish_time) noexcept;
    void calculate_prev_stat(SceneHandle scene) noexcept;

    constexpr static inline uint32_t calculate_work_groups_number(uint32_t threads_number, uint32_t group_size)
    {
      return (threads_number + group_size - 1) / group_size;
    }
  };
}
} // namespace mr
#endif // __MR_RENDER_CONTEXT_HPP_
