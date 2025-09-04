#include "window.hpp"
#include "camera/camera.hpp"
#include "render_context.hpp"

mr::Window::Window(const RenderContext &parent, Extent extent)
  : _extent(extent)
  , _parent(&parent)
{
  // --------------------------------------------------------------------------
  // Create window
  // --------------------------------------------------------------------------

  // TODO: retry a couple of times
  vkfw::WindowHints hints{};
  hints.resizable = true;
  hints.visible = true;
  // TODO(dk6): window options must be reworked, now it doesn't work
  // hints.focusOnShow = true;
  // hints.transparentFramebuffer = true;

  auto [result_code, window] =
    vkfw::createWindowUnique(_extent.width, _extent.height, "CGSGFOREVER", hints);
  if (result_code != vkfw::Result::eSuccess) {
    MR_FATAL("Cannot create window. vkfw::createWindowUnique failed with: {}", vkfw::to_string(result_code));
    std::exit(1);
  }

  _window = std::move(window);

  // --------------------------------------------------------------------------
  // Init render resources
  // --------------------------------------------------------------------------

  _surface = vkfw::createWindowSurfaceUnique(_parent->vulkan_state().instance(), _window.get());
  _swapchain = Swapchain(_parent->vulkan_state(), _surface.get(), _extent);

  for (int i = 0; i < _swapchain.value()._images.size(); i++) {
    _image_available_semaphore.emplace_back(_parent->vulkan_state().device().createSemaphoreUnique({}).value);
    _render_finished_semaphore.emplace_back(_parent->vulkan_state().device().createSemaphoreUnique({}).value);
  }

  // --------------------------------------------------------------------------
  // Setup window
  // --------------------------------------------------------------------------

  // glfwSetInputMode(_window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  _window->callbacks()->on_cursor_move = _input_state.get_mouse_callback();
  _window->callbacks()->on_key = _input_state.get_key_callback();
}

vk::RenderingAttachmentInfoKHR mr::Window::get_target_image() noexcept
{
  prev_image_index = image_index;
  auto device = _parent->vulkan_state().device();
  device.acquireNextImageKHR(_swapchain.value()._swapchain.get(),
                             UINT64_MAX,
                             _image_available_semaphore[image_index].get(),
                             nullptr,
                             &image_index);

  _swapchain.value()._images[image_index].switch_layout(_parent->vulkan_state(),
                                                        vk::ImageLayout::eColorAttachmentOptimal);

  return vk::RenderingAttachmentInfoKHR {
    .imageView = _swapchain.value()._images[image_index].image_view(),
    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    // TODO(dk6): added bckg color as parameter
    .clearValue = {vk::ClearColorValue( std::array {0.f, 0.f, 0.f, 0.f})},
  };
}

void mr::Window::present() noexcept
{
  _swapchain.value()._images[image_index].switch_layout(_parent->vulkan_state(), vk::ImageLayout::ePresentSrcKHR);
  std::array sems = {_render_finished_semaphore[image_index].get()};
  vk::PresentInfoKHR present_info {
    .waitSemaphoreCount = sems.size(),
    .pWaitSemaphores = sems.data(),
    .swapchainCount = 1,
    .pSwapchains = &_swapchain.value()._swapchain.get(),
    .pImageIndices = &image_index,
  };
  _parent->vulkan_state().queue().presentKHR(present_info);
}

vk::Semaphore mr::Window::image_ready_semaphore() noexcept
{
  return _image_available_semaphore[prev_image_index].get();
}

vk::Semaphore mr::Window::render_finished_semaphore() noexcept
{
  return _render_finished_semaphore[image_index].get();
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