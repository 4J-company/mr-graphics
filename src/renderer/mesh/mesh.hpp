#ifndef __MR_MESH_HPP_
#define __MR_MESH_HPP_

#include "pch.hpp"
#include "resources/resources.hpp"

namespace mr {

  class Mesh {
  private:
    std::vector<VertexBuffer> _vbufs;
    IndexBuffer _ibuf;

    std::atomic<int> _instance_count = 0;

  public:
    Mesh() = default;

    Mesh(std::vector<VertexBuffer>, IndexBuffer) noexcept;

    // move semantics
    Mesh(Mesh &&other) noexcept
    {
      _vbufs = std::move(other._vbufs);
      _ibuf = std::move(other._ibuf);
      _instance_count = std::move(other._instance_count.load());
    }

    Mesh &operator=(Mesh &&other) noexcept
    {
      _vbufs = std::move(other._vbufs);
      _ibuf = std::move(other._ibuf);
      _instance_count = std::move(other._instance_count.load());

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
      unit->bindIndexBuffer(_ibuf.buffer(), 0, _ibuf.index_type());
    }
    void draw(mr::CommandUnit &unit) const noexcept {
      unit->drawIndexed(_ibuf.element_count(), num_of_instances(), 0, 0, 0);
    }
  };
}     // namespace mr

#endif // __MR_MESH_HPP_
