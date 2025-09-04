#include "window.hpp"
#include "camera/camera.hpp"
#include "render_context.hpp"

vkfw::UniqueWindow mr::Window::_create_window(const Extent &extent) noexcept
{
  std::call_once(_init_vkfw_flag, [] {
    while (vkfw::init() != vkfw::Result::eSuccess) {}
  });

  // TODO: retry a couple of times
  vkfw::WindowHints hints {};
  hints.resizable = true;
  hints.visible = true;
  // TODO(dk6): window options must be reworked, now it doesn't work
  // hints.focusOnShow = true;
  // hints.transparentFramebuffer = true;

  auto [result_code, window] =
    vkfw::createWindowUnique(extent.width, extent.height, "CGSGFOREVER", hints);
  if (result_code != vkfw::Result::eSuccess) {
    MR_FATAL("Cannot create window. vkfw::createWindowUnique failed with: {}", vkfw::to_string(result_code));
    std::exit(1);
  }

  // glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  // TODO(dk6): without move compiler tries to call copy constructor, which is deleted
  return std::move(window);
}

mr::Window::Window(const RenderContext &parent, Extent extent)
  : Presenter(parent, extent)
  , _window(_create_window(_extent))
  , _surface(vkfw::createWindowSurfaceUnique(_parent->vulkan_state().instance(), _window.get()))
  , _swapchain(_parent->vulkan_state(), _surface.get(), _extent)
{
  for (uint32_t i = 0; i < _swapchain._images.size(); i++) {
    _image_available_semaphore.emplace_back(_parent->vulkan_state().device().createSemaphoreUnique({}).value);
    _render_finished_semaphore.emplace_back(_parent->vulkan_state().device().createSemaphoreUnique({}).value);
  }

  _window->callbacks()->on_cursor_move = _input_state.get_mouse_callback();
  _window->callbacks()->on_key = _input_state.get_key_callback();
}

vk::RenderingAttachmentInfoKHR mr::Window::target_image_info() noexcept
{
  prev_image_index = image_index;
  auto device = _parent->vulkan_state().device();
  device.acquireNextImageKHR(_swapchain._swapchain.get(),
                             UINT64_MAX,
                             _image_available_semaphore[prev_image_index].get(),
                             nullptr,
                             &image_index);

  _swapchain._images[image_index].switch_layout(_parent->vulkan_state(),
                                                        vk::ImageLayout::eColorAttachmentOptimal);

  _current_image_available_semaphore =_image_available_semaphore[prev_image_index].get();
  _current_render_finished_semaphore = _render_finished_semaphore[image_index].get();

  return vk::RenderingAttachmentInfoKHR {
    .imageView = _swapchain._images[image_index].image_view(),
    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    // TODO(dk6): added bckg color as parameter
    .clearValue = {vk::ClearColorValue( std::array {0.f, 0.f, 0.f, 0.f})},
  };
}

void mr::Window::present() noexcept
{
  _swapchain._images[image_index].switch_layout(_parent->vulkan_state(), vk::ImageLayout::ePresentSrcKHR);
  std::array sems = {_render_finished_semaphore[image_index].get()};
  vk::PresentInfoKHR present_info {
    .waitSemaphoreCount = sems.size(),
    .pWaitSemaphores = sems.data(),
    .swapchainCount = 1,
    .pSwapchains = &_swapchain._swapchain.get(),
    .pImageIndices = &image_index,
  };
  _parent->vulkan_state().queue().presentKHR(present_info);
}

void mr::Window::update_state() noexcept
{
  _input_state.update();

  if (_input_state.key_tapped(vkfw::Key::eEscape)) {
    _window->setShouldClose(true);
  }

  if (_input_state.key_tapped(vkfw::Key::eF11)) {
    if (_input_state.key_pressed(vkfw::Key::eLeftShift) ||
        _input_state.key_pressed(vkfw::Key::eRightShift)) {
      _window->setSize(640, 480);
    } else {
      _window->maximize();
    }
  }
}