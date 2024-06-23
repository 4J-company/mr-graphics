#ifndef __window_hpp_
#define __window_hpp_

#include "pch.hpp"
#include "window_context.hpp"

namespace mr {
  class Window {
    private:
      std::size_t _width, _height;
      vkfw::UniqueWindow _window;
      mr::WindowContext _context;

    public:
      // constructors
      Window(const mr::VulkanState &state, std::size_t width = 800,
             std::size_t height = 600);
      Window(Window &&other) noexcept = default;
      Window &operator=(Window &&other) noexcept = default;

      // destructor
      ~Window() = default;

      // getters
      size_t width() const { return _width; }

      size_t height() const { return _height; }

      vkfw::Window window() { return _window.get(); }

      // methods
      void start_main_loop();
      void render() { _context.render(); }
  };
} // namespace mr
#endif // __window_hpp_
