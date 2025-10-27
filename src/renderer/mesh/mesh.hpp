#ifndef __MR_MESH_HPP_
#define __MR_MESH_HPP_

#include "pch.hpp"
#include "resources/resources.hpp"
#include <vulkan/vulkan_core.h>

namespace mr {
inline namespace graphics {
  class Model;
  class RenderContext;
  class Scene;

  class Mesh {
    friend class Model;
    friend class RenderContext;
    friend class Scene;

  public:
    struct RenderInfo {
      uint32_t mesh_offset;
      uint32_t instance_offset;
      uint32_t material_ubo_id;
      uint32_t camera_buffer_id;
      uint32_t transforms_buffer_id;
    };

  private:
    VertexBuffersArray _vbufs;
    std::vector<IndexBufferDescription> _ibufs;

    std::atomic<uint32_t> _instance_count = 0;

    uint32_t _mesh_offset = 0;     // offset to the *per mesh*     data buffer in the scene
    uint32_t _instance_offset = 0; // offset to the *per instance* data buffer in the scene

  public:
    Mesh() = default;

    Mesh(VertexBuffersArray vbufs,
         std::vector<IndexBufferDescription> ibufs,
         size_t instance_count,
         size_t mesh_offset,
         size_t instance_offset) noexcept;

    // move semantics
    Mesh(Mesh &&other) noexcept { *this = std::move(other); }

    Mesh & operator=(Mesh &&other) noexcept
    {
      _vbufs = std::move(other._vbufs);
      _ibufs = std::move(other._ibufs);
      _instance_count = std::move(other._instance_count.load());
      _mesh_offset = std::move(other._mesh_offset);
      _instance_offset = std::move(other._instance_offset);

      return *this;
    }

    std::atomic<uint32_t> & num_of_instances() noexcept { return _instance_count; }
    uint32_t num_of_instances() const noexcept { return _instance_count.load(); }
    // uint32_t element_count() const noexcept { return _ibufs[0].element_count(); }
    uint32_t element_count() const noexcept { return _ibufs[0].elements_count; }
  };
}
}     // namespace mr

#endif // __MR_MESH_HPP_
