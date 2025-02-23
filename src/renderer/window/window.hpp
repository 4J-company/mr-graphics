#ifndef __MR_WINDOW_HPP_
#define __MR_WINDOW_HPP_

#include "pch.hpp"
#include "render_context.hpp"
#include "utils/misc.hpp"
#include "camera/camera.hpp"

namespace mr {
  class Window {
    private:
      Extent _extent;
      vkfw::UniqueWindow _window;
      mr::RenderContext _context;
      mr::FPSCamera *_cam;

    public:
      Window(VulkanGlobalState *state, Extent extent = {800, 600});

      Window(Window &&other) noexcept = default;
      Window &operator=(Window &&other) noexcept = default;
      ~Window() = default;

      Extent extent() const noexcept { return _extent; }

      vkfw::Window window() { return _window.get(); }

      // methods
      void start_main_loop();
      void render() { _context.render(*_cam); }
  };
} // namespace mr
#endif // __MR_WINDOW_HPP_
