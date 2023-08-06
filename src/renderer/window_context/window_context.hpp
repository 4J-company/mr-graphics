#if !defined(__window_context_hpp_)
#define __window_context_hpp_

#include "pch.hpp"
#include "window_system/window.hpp"

namespace ter 
{
class application;
class window_context;

class framebuffer 
{
  friend class window_context;

public:
  inline static const uint32_t num_of_presentable_images = 3,
                               num_of_gbuffers = 1;                 

private:
  std::array<vk::Image, num_of_presentable_images> _swapchain_images;
  std::array<vk::ImageView, num_of_presentable_images> _swapchain_images_views;

  std::array<vk::Image, num_of_gbuffers> _gbuffers;
  std::array<vk::ImageView, num_of_gbuffers> _gbuffers_views;

public:
  framebuffer() = default;
  framebuffer(const vk::Format &image_format, const vk::Device &device);

  ~framebuffer();

  void resize(size_t width, size_t height);
};

class window_context 
{
  friend class application;

private:
  vk::SwapchainKHR _swapchain;
  vk::Format _swapchain_format;
  vk::UniqueSurfaceKHR _surface;

  framebuffer _framebuffer;

public:
  window_context(window_system::window *window, const application &app);

  window_context() = default;

  void resize(size_t width, size_t height);
};
} // namespace ter
#endif // __window_context_hpp_
