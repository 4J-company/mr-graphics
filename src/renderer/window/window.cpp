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

  // TODO(dk6): camera must be reworked, now it doesn't work

  // glfwSetInputMode(_window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  // _window->callbacks()->on_cursor_move =
  //   [this](const vkfw::Window &win, double x, double y) {
  //   static mr::Vec2d old_pos;
  //   mr::Vec2d pos = {x, y};
  //   mr::Vec2d delta = pos - old_pos;
  //   old_pos = pos;
  //   camera.turn({delta.x() / _extent.width, delta.y() / _extent.height, 0});
  // };

  _window->callbacks()->on_key = [&](const vkfw::Window &win, vkfw::Key key,
                                    int scan_code, vkfw::KeyAction action,
                                    vkfw::ModifierKeyFlags flags) {
    if (key == vkfw::Key::eEscape) {
      win.setShouldClose(true);
    }

    // TODO(dk6): I moved camera in Scene. It need to be upgraded... Maybe attach actual Scene to Window?
    // camera controls
    // if (key == vkfw::Key::eW) {
    //   camera.move(camera.cam().direction());
    // }
    // if (key == vkfw::Key::eA) {
    //   camera.move(-camera.cam().right());
    // }
    // if (key == vkfw::Key::eS) {
    //   camera.move(-camera.cam().direction());
    // }
    // if (key == vkfw::Key::eD) {
    //   camera.move(camera.cam().right());
    // }
    // if (key == vkfw::Key::eSpace) {
    //   camera.move(-camera.cam().up());
    // }
    // if (key == vkfw::Key::eLeftShift) {
    //   camera.move(camera.cam().up());
    // }

    if (key == vkfw::Key::eF11) {
      if (flags & vkfw::ModifierKeyBits::eShift) {
        win.setSize(640, 480);
      }
      else {
        win.maximize();
      }
    }
  };
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
