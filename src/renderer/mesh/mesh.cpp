#include "mesh/mesh.hpp"

mr::graphics::Mesh::Mesh(
	std::vector<VertexBuffer> vbufs,
	IndexBuffer ibuf) noexcept
  : _ibuf(std::move(ibuf))
  , _vbufs(std::move(vbufs))
  , _instance_count(1)
{
}
