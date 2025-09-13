#ifndef __MR_MESH_HPP_
#define __MR_MESH_HPP_

#include "pch.hpp"
#include "resources/resources.hpp"

namespace mr {
inline namespace graphics {
  class Model;

  class Mesh {
    friend class Model;

  private:
    std::vector<VertexBuffer> _vbufs;
    std::vector<IndexBuffer> _ibufs;

    std::atomic<int> _instance_count = 0;

    uint32_t _mesh_offset = 0;     // offset to the *per mesh*     data buffer in the scene
    uint32_t _instance_offset = 0; // offset to the *per instance* data buffer in the scene

  public:
    Mesh() = default;

    Mesh(std::vector<VertexBuffer>, std::vector<IndexBuffer>, size_t, size_t, size_t) noexcept;

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

    std::atomic<int> &num_of_instances() noexcept { return _instance_count; }
    int num_of_instances() const noexcept { return _instance_count.load(); }
    void bind(mr::CommandUnit &unit) const noexcept {
      static std::array<vk::Buffer, 16> vbufs {};
      static std::array<vk::DeviceSize, 16> offsets {};

      for (int i = 0; i < _vbufs.size(); i++) {
        vbufs[i] = _vbufs[i].buffer();
      }

      unit->bindVertexBuffers(0,
          std::span{vbufs.data(), _vbufs.size()},
          std::span{offsets.data(), _vbufs.size()});
      unit->bindIndexBuffer(_ibufs[0].buffer(), 0, _ibufs[0].index_type());
    }

    void draw(mr::CommandUnit &unit) const noexcept {
      unit->drawIndexed(_ibufs[0].element_count(), num_of_instances(), 0, 0, 0);
    }
  };
}
}     // namespace mr

#endif // __MR_MESH_HPP_
