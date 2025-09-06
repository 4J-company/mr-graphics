#ifndef __MR_WINDOW_CONTEXT_HPP_
#define __MR_WINDOW_CONTEXT_HPP_

#include "pch.hpp"
#include "resources/images/image.hpp"
#include "resources/resources.hpp"
#include "vulkan_state.hpp"
#include "camera/camera.hpp"
#include "swapchain.hpp"
#include "lights/light_render_data.hpp"

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

    private:
      Window *_parent;
      std::shared_ptr<VulkanState> _state;
      Extent _extent;

      vk::UniqueSurfaceKHR _surface;
      Swapchain _swapchain;

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

    public:
      RenderContext(RenderContext &&other) noexcept = default;
      RenderContext & operator=(RenderContext &&other) noexcept = default;

      RenderContext(const RenderContext &other) noexcept = delete;
      RenderContext & operator=(const RenderContext &other) noexcept = delete;

      RenderContext(VulkanGlobalState *global_state, Window *parent);

      ~RenderContext();

      void resize(Extent extent);
      void render(mr::FPSCamera &cam);

    private:
      void _init_lights_render_data();

      void render_models(UniformBuffer &cam_ubo, CommandUnit &command_unit, mr::FPSCamera &cam);
      void render_lights(UniformBuffer &cam_ubo, CommandUnit &command_unit, uint32_t image_index);
  };
}
} // namespace mr
#endif // __MR_WINDOW_CONTEXT_HPP_
