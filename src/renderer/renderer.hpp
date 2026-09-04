#ifndef __MR_RENDERER_HPP_
#define __MR_RENDERER_HPP_

#include "pch.hpp"

#include "mesh/attribute_types.hpp"
#include "resources/resources.hpp"
#include "timer/timer.hpp"
#include "vulkan_state.hpp"
#include "window/presenter.hpp"
#include "window/window.hpp"
#include "window/file_writer.hpp"
#include "scene/scene.hpp"
#include "window/render_context.hpp"

#include "mesh/mesh.hpp"

namespace mr {
inline namespace graphics {
  class Application {
  private:
    VulkanGlobalState _state;

  public:
    Application(bool init_vkfw = true);
    ~Application();

    [[nodiscard]] std::unique_ptr<RenderContext>
    create_render_context(Extent extent, RenderContextConfig config = {});

    void start_render_loop(RenderContext &render_context, SceneHandle scene, WindowHandle window,
                           std::optional<std::reference_wrapper<std::ostream>> stat_log_stream = std::nullopt,
                           std::optional<uint32_t> frame_number = std::nullopt) const noexcept;

    void start_render_loop(RenderContext &render_context, SceneHandle scene, WindowHandle window,
                           std::optional<std::reference_wrapper<std::ostream>> stat_log_stream,
                           std::optional<uint32_t> frame_number,
                           std::optional<uint32_t> save_gbuffer_frame,
                           std::optional<std::fs::path> save_gbuffer_path) const noexcept;

    void render_frames(RenderContext &render_context,
                       SceneHandle scene,
                       FileWriterHandle file_writer,
                       std::fs::path dst_dir = "",
                       std::string_view filename_prefix = "frame",
                       std::optional<uint32_t> frames = std::nullopt) const noexcept;
  };
}
} // namespace mr

#endif // __MR_RENDERER_HPP_
