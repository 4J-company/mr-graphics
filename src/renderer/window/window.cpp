#include "window.hpp"
#include "render_context.hpp"
#include "camera/camera.hpp"

void mr::Window::update_swapchain() noexcept
{
  // Don't touch this loop.
  // If errors occur during resize - it's this bastard right under here
  while (_should_update_swapchain) {
    _should_update_swapchain = false;

    _parent->vulkan_state().device().waitIdle();

    vkb::destroy_swapchain(_swapchain._swapchain);
    vkb::destroy_surface(_parent->vulkan_state().instance(), _surface.release());

    _surface = vkfw::createWindowSurfaceUnique(_parent->vulkan_state().instance(), _window.get());

    auto [res, value] = _window->getSize();
    auto [w, h] = value;
    _extent.width = w;
    _extent.height = h;

    _swapchain = mr::Swapchain(_parent->vulkan_state(), _surface.get(), _extent);
  }
}

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
  _window->callbacks()->on_window_resize = [this](const vkfw::Window&, uint32_t, uint32_t) { _should_update_swapchain = true; };
}

mr::Window::~Window() {
  if (_parent) {
    _parent->vulkan_state().queue().waitIdle();

    _image_available_semaphore.clear();
    _render_finished_semaphore.clear();

    vkb::destroy_swapchain(_swapchain._swapchain);
    vkb::destroy_surface(_parent->vulkan_state().instance(), _surface.release());
  }
}

vk::RenderingAttachmentInfoKHR mr::Window::target_image_info() noexcept
{
  std::optional<vk::RenderingAttachmentInfoKHR> res = target_image_info_impl();
  while (!res) { res = target_image_info_impl(); }
  return res.value();
}

std::optional<vk::RenderingAttachmentInfoKHR> mr::Window::target_image_info_impl() noexcept
{
  while (_should_update_swapchain) update_swapchain();

  prev_image_index = image_index;
  auto device = _parent->vulkan_state().device();
  auto result = device.acquireNextImageKHR(_swapchain,
                             UINT64_MAX,
                             _image_available_semaphore[prev_image_index].get(),
                             nullptr,
                             &image_index);
  if (result == vk::Result::eErrorOutOfDateKHR) {
    _should_update_swapchain = true;
    return std::nullopt;
  }

  _swapchain._images[image_index].switch_layout(vk::ImageLayout::eColorAttachmentOptimal);

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
  _swapchain._images[image_index].switch_layout(vk::ImageLayout::ePresentSrcKHR);
  std::array sems = {_render_finished_semaphore[image_index].get()};
  vk::PresentInfoKHR present_info {
    .waitSemaphoreCount = sems.size(),
    .pWaitSemaphores = sems.data(),
    .swapchainCount = 1,
    .pSwapchains = (vk::SwapchainKHR*)&_swapchain._swapchain.swapchain,
    .pImageIndices = &image_index,
  };
  auto result = _parent->vulkan_state().queue().presentKHR(present_info);
  if (result == vk::Result::eErrorOutOfDateKHR) {
    _should_update_swapchain = true;
    return;
  }
  ASSERT(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR, "Failed to present onto a window", result);
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
