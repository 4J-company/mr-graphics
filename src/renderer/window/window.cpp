module;
#include "pch.hpp"
export module Window;

export import WindowContext;

export namespace mr {
  class Window {
    private:
      std::size_t _width, _height;
      std::jthread _thread;

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
      void render() { _context.render(); }
  };
} // namespace mr

// size-based constructor
// arguments:
//   - size:
//       size_t width, height
mr::Window::Window(const VulkanState &state, size_t width, size_t height)
    : _width(width)
    , _height(height)
{
  // TODO: retry a couple of times
  auto [result_code, window] =
    vkfw::createWindowUnique(640, 480, "CGSGFOREVER");
  if (result_code != vkfw::Result::eSuccess) {
    exit(1);
  }
  _window = std::move(window);

  _thread = std::jthread([&]() {
    while (!_window->shouldClose().value) {}
  });

  _context = mr::WindowContext(this, state);
} // End of 'wnd::window::window' function
