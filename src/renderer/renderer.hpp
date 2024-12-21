#include "mesh/attribute_types.hpp"
#if !defined(__renderer_hpp_)
#define __renderer_hpp_

#include "pch.hpp"

#include "resources/resources.hpp"
#include "timer/timer.hpp"
#include "vulkan_state.hpp"
#include "window/window.hpp"

#include "mesh/mesh.hpp"

namespace mr {
  class Application {
    private:
      VulkanGlobalState _state;

      /// TMP SOLUTION
      mutable std::vector<mr::Mesh> _tmp_mesh_pool;

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
} // namespace mr

#endif // __renderer_hpp_
