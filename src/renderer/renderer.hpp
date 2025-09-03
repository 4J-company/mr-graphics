#ifndef __MR_RENDERER_HPP_
#define __MR_RENDERER_HPP_

#include "pch.hpp"

#include "mesh/attribute_types.hpp"
#include "resources/resources.hpp"
#include "timer/timer.hpp"
#include "vulkan_state.hpp"
#include "window/window.hpp"
#include "scene/scene.hpp"

#include "mesh/mesh.hpp"

namespace mr {
  class Application {
    private:
      VulkanGlobalState _state;

    public:
      Application();
      ~Application();

      [[nodiscard]] std::unique_ptr<RenderContext> create_render_context(Extent extent);

      void start_render_loop(RenderContext &render_context, const SceneHandle scene,
                                                            WindowHandle window) const noexcept;
  };
} // namespace mr

#endif // __MR_RENDERER_HPP_
