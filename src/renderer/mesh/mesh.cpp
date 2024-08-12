#include "mesh/mesh.hpp"

mr::Mesh::Mesh(
	VertexBuffer vbuf,
	IndexBuffer ibuf) noexcept
  : _ibuf(std::move(ibuf))
  , _vbuf(std::move(vbuf))
  , _instance_count(1)
{
}
