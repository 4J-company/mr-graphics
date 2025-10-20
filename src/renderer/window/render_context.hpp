#ifndef __MR_RENDER_CONTEXT_HPP_
#define __MR_RENDER_CONTEXT_HPP_

#include "pch.hpp"
#include "resources/images/image.hpp"
#include "resources/resources.hpp"
#include "vulkan_state.hpp"
#include "camera/camera.hpp"
#include "lights/lights.hpp"
#include "model/model.hpp"
#include "window.hpp"
#include "file_writer.hpp"
#include "scene/scene.hpp"
#include "resources/command_unit/command_unit.hpp"

#include <VkBootstrap.h>

namespace mr {
inline namespace graphics {
  class Window;

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
    constexpr static uint32_t textures_binding = 0;
    constexpr static uint32_t uniform_buffer_binding = 1;
    constexpr static uint32_t storage_buffer_binding = 2;

  private:
    std::shared_ptr<VulkanState> _state;
    Extent _extent;

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

    // semaphore for sync opaque models rendering and light shading
    vk::UniqueSemaphore _models_render_finished_semaphore;
    vk::UniqueFence _image_fence; // fence for swapchain image?

    LightsRenderData _lights_render_data;

    // TODO(dk6): Maybe move to class scene
    // Bindless rednering data
    DescriptorAllocator _default_descriptor_allocator;
    BindlessDescriptorSetLayoutHandle _bindless_set_layout;
    BindlessDescriptorSet _bindless_set;

    DynamicVertexBuffer _positions_vertex_buffer;
    DynamicVertexBuffer _attributes_vertex_buffer;
    DynamicIndexBuffer _index_buffer;

  public:
    RenderContext(RenderContext &&other) noexcept = default;
    RenderContext & operator=(RenderContext &&other) noexcept = default;

    RenderContext(const RenderContext &other) noexcept = delete;
    RenderContext & operator=(const RenderContext &other) noexcept = delete;

    // TODO(dk6): change pointer to reference
    RenderContext(VulkanGlobalState *global_state, Extent extent);

    ~RenderContext();

    void resize(const mr::Extent &extent);

    void render(const SceneHandle scene, Presenter &presenter);

    // ===== Getters =====
    const LightsRenderData & lights_render_data() const noexcept { return _lights_render_data; }
    const VulkanState & vulkan_state() const noexcept { return *_state; }
    const Extent & extent() const noexcept { return _extent; }
    CommandUnit & transfer_command_unit() const noexcept { return _transfer_command_unit; }

    DynamicVertexBuffer & positions_vertex_buffer() noexcept { return _positions_vertex_buffer; }
    DynamicVertexBuffer & attributes_vertex_buffer() noexcept { return _attributes_vertex_buffer; }
    DynamicIndexBuffer & index_buffer() noexcept { return _index_buffer; }

    // ===== Resources creation =====
    WindowHandle create_window() const noexcept;
    WindowHandle create_window(const mr::Extent &extent) const noexcept;
    FileWriterHandle create_file_writer() const noexcept;

    SceneHandle create_scene() noexcept;

    // ===== Bindless rendering =====
    DescriptorSetLayoutHandle bindless_set_layout() const noexcept { return _bindless_set_layout; }
    BindlessDescriptorSet & bindless_set() noexcept { return _bindless_set; }
    const BindlessDescriptorSet & bindless_set() const noexcept { return _bindless_set; }

    const DescriptorAllocator & desciptor_allocator() const noexcept { return _default_descriptor_allocator; }

  private:
    void init_lights_render_data();
    void init_bindless_rendering();

    void render_models(const SceneHandle scene);
    void render_lights(const SceneHandle scene, Presenter &presenter);

    void update_camera_buffer(UniformBuffer &uniform_buffer);
  };
}
} // namespace mr
#endif // __MR_RENDER_CONTEXT_HPP_
