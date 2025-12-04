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
    double culling_gpu_time_ms = 0;
    double render_gpu_time_ms = 0;
    double models_gpu_time_ms = 0;
    double shading_gpu_time_ms = 0;

    double gpu_time_ms = 0;
    double gpu_fps = 0;

    double render_cpu_time_ms = 0;
    double models_cpu_time_ms = 0;
    double shading_cpu_time_ms = 0;

    double cpu_time_ms = 0; // time between calls of render
    double cpu_fps = 0;

    uint64_t vertexes_number = 0;
    uint64_t triangles_number = 0;

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

    // Bindings numbers in bindless descriptor set
    constexpr static inline uint32_t textures_binding = 0;
    constexpr static inline uint32_t uniform_buffer_binding = 1;
    constexpr static inline uint32_t storage_buffer_binding = 2;
    constexpr static inline uint32_t bindless_set_number = 0;

    constexpr static inline uint32_t default_vertex_number = 10'000'000;
    constexpr static inline uint32_t default_index_number = default_vertex_number * 2;

    constexpr static inline uint32_t culling_work_group_size = 32;

  private:
    // Timestamps
    enum struct Timestamp : uint32_t {
      CullingStart,
      CullingEnd,
      ModelsStart,
      ModelsEnd,
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
      uint32_t bound_box_index;
    };

  private:
    std::shared_ptr<VulkanState> _state;
    Extent _extent;
    RenderOptions _render_options;

    vk::UniqueQueryPool _timestamps_query_pool {};
    RenderStat _render_stat;
    ClockT::time_point _prev_start_time {};
    uint64_t _prev_first_timestamp = 0;
    double _timestamp_to_ms = 0;

    TracyVkCtx _models_tracy_gpu_context {};
    TracyVkCtx _lights_tracy_gpu_context {};

    CommandUnit _models_command_unit;
    CommandUnit _lights_command_unit;
    // RenderContext doesn't use transfer command unit, only gives it for buffers
    // Writting commands to it doesn't affect RenderContext internal state
    mutable CommandUnit _transfer_command_unit;

    // TODO(dk6): use Framedata instead
    InplaceVector<ColorAttachmentImage, gbuffers_number> _gbuffers;
    DepthImage _depthbuffer;

    // semaphores for waiting swapchain image is ready before light pass
    InplaceVector<vk::UniqueSemaphore, max_images_number> _image_available_semaphore;
    // semaphores for waiting frame is ready before presentin
    InplaceVector<vk::UniqueSemaphore, max_images_number> _render_finished_semaphore;

    vk::UniqueSemaphore _pre_model_layout_transition_semaphore;
    CommandUnit _pre_model_layout_transition_command_unit;

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
    vk::UniqueSemaphore _culling_semaphore;
    ShaderHandle _instances_culling_shader;
    ComputePipeline _instances_culling_pipeline;
    ShaderHandle _instances_collect_shader;
    ComputePipeline _instances_collect_pipeline;

    Extent _depth_pyramid_extent;
    PyramidImage _depth_pyramid;
    ComputePipeline _depth_pyramid_pipeline;
    ShaderHandle _depth_pyramid_shader;

    // TODO(dk6): rework it to MarkerSystem
    ShaderHandle _bound_boxes_draw_shader;
    GraphicsPipeline _bound_boxes_draw_pipeline;
    StorageBuffer _bound_boxes_buffer;
    uint32_t _bound_boxes_buffer_id = -1;
    std::vector<BoundBoxRenderData> _bound_boxes_data;
    std::atomic_bool _bound_boxes_data_dirty = false;
    std::atomic_bool _bound_boxes_draw_enabled = false;

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
    CommandUnit & transfer_command_unit() const noexcept { return _transfer_command_unit; }

    void enable_bound_boxes() noexcept { _bound_boxes_draw_enabled = true; }
    void disable_bound_boxes() noexcept { _bound_boxes_draw_enabled = false; }

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
                        uint32_t bound_boxes_buffer_id, uint32_t bound_box_index) noexcept;

  private:
    void init_lights_render_data();
    void init_bindless_rendering();
    void init_profiling();
    void init_culling();
    void init_bound_box_rendering();
    void init_depth_pyramid();

    void render_geometry(const SceneHandle scene);
    void culling_geometry(const SceneHandle scene);
    void build_depth_pyramid();
    void render_bound_boxes(const SceneHandle scene);
    void render_models(const SceneHandle scene);
    void render_lights(const SceneHandle scene, Presenter &presenter);

    void update_bound_boxes_data();
    void update_camera_buffer(UniformBuffer &uniform_buffer);

    void calculate_stat(SceneHandle scene,
                        ClockT::time_point render_start_time,
                        ClockT::time_point render_finish_time,
                        ClockT::duration models_time,
                        ClockT::duration shading_time);

    constexpr static inline uint32_t calculate_work_groups_number(uint32_t threads_number, uint32_t group_size)
    {
      return (threads_number + group_size - 1) / group_size;
    }
  };
}
} // namespace mr
#endif // __MR_RENDER_CONTEXT_HPP_
