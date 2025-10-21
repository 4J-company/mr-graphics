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
#if USE_MERGED_VERTEX_BUFFER
    std::vector<uint32_t> _vbufs;
#else // USE_MERGED_VERTEX_BUFFER
    std::vector<VertexBuffer> _vbufs;
#endif // USE_MERGED_VERTEX_BUFFER
#if USE_MERGED_INDEX_BUFFER
    std::vector<std::pair<uint32_t, uint32_t>> _ibufs;
#else // USE_MERGED_INDEX_BUFFER
    std::vector<IndexBuffer> _ibufs;
#endif


    std::atomic<uint32_t> _instance_count = 0;

    uint32_t _mesh_offset = 0;     // offset to the *per mesh*     data buffer in the scene
    uint32_t _instance_offset = 0; // offset to the *per instance* data buffer in the scene

  public:
    Mesh() = default;

    Mesh(
#if USE_MERGED_VERTEX_BUFFER
      std::vector<uint32_t> vertex_buffers_offsets,
#else // USE_MERGED_VERTEX_BUFFER
	    std::vector<VertexBuffer> vbufs,
#endif // USE_MERGED_VERTEX_BUFFER
#if USE_MERGED_INDEX_BUFFER
      std::vector<std::pair<uint32_t, uint32_t>> ibufs,
#else // USE_MERGED_INDEX_BUFFER
      std::vector<IndexBuffer> ibufs,
#endif // USE_MERGED_INDEX_BUFFER
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
    uint32_t element_count() const noexcept
    {
#if USE_MERGED_INDEX_BUFFER
      return _ibufs[0].second;
#else // USE_MERGED_INDEX_BUFFER
      return _ibufs[0].element_count();
#endif // USE_MERGED_INDEX_BUFFER
    }

    // TODO(dk6): After union all in one big vertex buffer and we can delete this funciton
    void bind(mr::CommandUnit &unit) const noexcept;
  };
}
}     // namespace mr

#endif // __MR_MESH_HPP_
