#ifndef __MR_MESH_HPP_
#define __MR_MESH_HPP_

#include "pch.hpp"
#include "resources/resources.hpp"

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
    std::vector<VertexBuffer> _vbufs;
    // std::vector<IndexBuffer> _ibufs;

    // std::vector<uint32_t> _vbufs;
    std::vector<std::pair<uint32_t, uint32_t>> _ibufs;

    std::atomic<uint32_t> _instance_count = 0;

    uint32_t _mesh_offset = 0;     // offset to the *per mesh*     data buffer in the scene
    uint32_t _instance_offset = 0; // offset to the *per instance* data buffer in the scene

  public:
    Mesh() = default;

    // Mesh(std::vector<VertexBuffer>, std::vector<IndexBuffer>, size_t, size_t, size_t) noexcept;
    Mesh(
      // std::vector<uint32_t> vertex_buffers_offsets,
	    std::vector<VertexBuffer> vertex_buffers_offsets,
         std::vector<std::pair<uint32_t, uint32_t>> index_buffers_offsets_sizes,
         size_t instance_count,
         size_t mesh_offset,
         size_t instance_offset) noexcept;

    // move semantics
    Mesh(Mesh &&other) noexcept
    {
      _vbufs = std::move(other._vbufs);
      _ibufs = std::move(other._ibufs);
      _instance_count = std::move(other._instance_count.load());
      _mesh_offset = std::move(other._mesh_offset);
      _instance_offset = std::move(other._instance_offset);
    }

    Mesh &operator=(Mesh &&other) noexcept
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
    uint32_t element_count() const noexcept { return _ibufs[0].second; }

    // TODO(dk6): After union all in one big vertex buffer and we can delete this funciton
    void bind(mr::CommandUnit &unit) const noexcept;
  };
}
}     // namespace mr

#endif // __MR_MESH_HPP_
