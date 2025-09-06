#ifndef __MR_RENDERER_HPP_
#define __MR_RENDERER_HPP_

#include "pch.hpp"

#include "mesh/attribute_types.hpp"
#include "resources/resources.hpp"
#include "timer/timer.hpp"
#include "vulkan_state.hpp"
#include "window/window.hpp"

#include "mesh/mesh.hpp"

namespace mr {
inline namespace graphics {
  class Application {
    private:
      VulkanGlobalState _state;

    public:
      Application();
      ~Application();

      // resource creators
      [[nodiscard]] std::unique_ptr<CommandUnit> create_command_unit() const;
      [[nodiscard]] std::unique_ptr<HostBuffer> create_host_buffer() const;
      [[nodiscard]] std::unique_ptr<DeviceBuffer> create_device_buffer() const;
      [[nodiscard]] std::unique_ptr<Shader> create_shader() const;
      [[nodiscard]] std::unique_ptr<Pipeline> create_graphics_pipeline() const;
      [[nodiscard]] std::unique_ptr<Pipeline> create_compute_pipeline() const;

      // window creator
      [[nodiscard]] std::unique_ptr<Window> create_window(Extent extent);
  };
}
} // namespace mr

#endif // __MR_RENDERER_HPP_
