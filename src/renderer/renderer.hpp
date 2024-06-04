#include "mesh/attribute_types.hpp"
#if !defined(__renderer_hpp_)
  #define __renderer_hpp_

  #include "pch.hpp"

  #include "resources/resources.hpp"
  #include "timer/timer.hpp"
  #include "vulkan_application.hpp"
  #include "window/window.hpp"

  #include "mesh/mesh.hpp"

namespace mr
{
  class Application
  {
  private:
    VulkanState _state;
    VkDebugUtilsMessengerEXT _debug_messenger;

    /// TMP SOLUTION
    mutable std::vector<mr::Mesh> _tmp_mesh_pool;

  public:
    Application();
    ~Application();

    // resource creators
    [[nodiscard]] std::unique_ptr<CommandUnit> create_command_unit() const;
    [[nodiscard]] std::unique_ptr<Buffer> create_buffer() const;
    [[nodiscard]] std::unique_ptr<Shader> create_shader() const;
    [[nodiscard]] std::unique_ptr<Pipeline> create_graphics_pipeline() const;
    [[nodiscard]] std::unique_ptr<Pipeline> create_compute_pipeline() const;
    [[nodiscard]] Mesh * create_mesh(
        std::span<PositionType> positions,
        std::span<FaceType> faces,
        std::span<ColorType> colors,
        std::span<TexCoordType> uvs,
        std::span<NormalType> normals,
        std::span<NormalType> tangents,
        std::span<NormalType> bitangent,
        std::span<BoneType> bones,
        BoundboxType bbox
        ) const;

    // window creator
    [[nodiscard]] std::unique_ptr<Window> create_window(size_t width, size_t height) const;
  };
} // namespace mr

#endif // __renderer_hpp_
