#include "pch.hpp"

ter::window_context::window_context( window_system::window *window, vk::Instance instance )
{
  // Get the GDK window handle
  GdkWindow* gdkWindow = window->gobj();
  const uint32_t imageCount = 3;
  vk::SurfaceTransformFlagBitsKHR surfaceTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
  vk::CompositeAlphaFlagBitsKHR compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  vk::Format imageFormat = vk::Format::eB8G8R8A8Unorm;
  
  // Create the VkSurfaceCreateInfo struct
  vk::SurfaceCreateInfoKHR createInfo = {};
  createInfo.setSurfaceType(vk::SurfaceTransformFlagBitsKHR::eIdentity);
  createInfo.setMinImageCount(3);
  createInfo.setImageFormat(vk::Format::eB8G8R8A8Unorm);
  createInfo.setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear);
  createInfo.setImageExtent(vk::Extent2D(window->get_width(), window->get_height()));
  createInfo.setPresentMode(vk::PresentModeKHR::eFifo);
  createInfo.setClipped(true);

  // Check if the window is a Wayland window
  if (GDK_IS_WAYLAND_WINDOW(gdkWindow)) {
    createInfo.setSurfaceKHR(vk::WaylandSurfaceCreateInfoKHR({}, gdk_wayland_window_get_wl_surface(GDK_WAYLAND_WINDOW(gdkWindow))));
    _surface = instance.createWaylandSurfaceKHR(createInfo);
  }
  // Check if the window is an X11 window
  else if (GDK_IS_X11_WINDOW(gdkWindow)) {
    createInfo.setSurfaceKHR(vk::XlibSurfaceCreateInfoKHR({}, gdk_x11_window_get_xid(GDK_X11_WINDOW(gdkWindow)), nullptr));
    _surface = instance.createXlibSurfaceKHR(createInfo);
  }

  // Create the swapchain create info
  vk::SwapchainCreateInfoKHR createInfo = {};
  createInfo.setSurface(_surface.get());
  createInfo.setMinImageCount(imageCount);
  createInfo.setImageFormat(imageFormat);
  createInfo.setImageColorSpace(surfaceFormat.colorSpace);
  createInfo.setImageExtent(extent);
  createInfo.setImageArrayLayers(1);
  createInfo.setImageUsage(usageFlags);
  createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
  createInfo.setPreTransform(surfaceTransform);
  createInfo.setCompositeAlpha(compositeAlpha);
  createInfo.setPresentMode(presentMode);
  createInfo.setClipped(true);
  // Create the swapchain
  _swapchain = device.createSwapchainKHRUnique(createInfo);
}
