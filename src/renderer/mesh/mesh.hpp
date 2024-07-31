#ifndef __mesh_cpp_
#define __mesh_cpp_

#include "pch.hpp"
#include "resources/resources.hpp"

namespace mr {

  class Mesh {
  private:
    VertexBuffer _vbuf;
    IndexBuffer _ibuf;

    size_t _element_count = 0;

    std::atomic<int> _instance_count = 0;

  public:
    Mesh() = default;

    Mesh(VertexBuffer, IndexBuffer, std::size_t) noexcept;

    // move semantics
    Mesh(Mesh &&other) noexcept
    {
      _vbuf = std::move(other._vbuf);
      _ibuf = std::move(other._ibuf);
      _element_count = std::move(other._element_count);
      _instance_count = std::move(other._instance_count.load());
    }

    Mesh &operator=(Mesh &&other) noexcept
    {
      _vbuf = std::move(other._vbuf);
      _ibuf = std::move(other._ibuf);
      _element_count = std::move(other._element_count);
      _instance_count = std::move(other._instance_count.load());

      return *this;
    }

    unsigned num_of_indices() const noexcept { return _element_count; }
    const VertexBuffer & vbuf() const noexcept { return _vbuf; }
    const IndexBuffer & ibuf() const noexcept { return _ibuf; }
  };
}     // namespace mr

#endif // __mesh_cpp_
