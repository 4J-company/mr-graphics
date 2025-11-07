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

#include "mesh/mesh.hpp"

namespace mr {
inline namespace graphics {
  class Application {
    private:
      VulkanGlobalState _state;

    public:
      Application(bool init_vkfw = true);
      ~Application();

      [[nodiscard]] std::unique_ptr<RenderContext> create_render_context(Extent extent);

      void start_render_loop(RenderContext &render_context, SceneHandle scene,
                                                            WindowHandle window) const noexcept;

      void render_frames(RenderContext &render_context,
                         SceneHandle scene,
                         FileWriterHandle file_writer,
                         std::fs::path dst_dir = "",
                         std::string_view filename_prefix = "frame",
                         uint32_t frames = 1) const noexcept;
  };
}
} // namespace mr

#endif // __MR_RENDERER_HPP_
