#include "mesh/mesh.hpp"

mr::Mesh::Mesh(
	VertexBuffer vbuf,
	IndexBuffer ibuf,
	std::size_t index_count) noexcept
  : _ibuf(std::move(ibuf))
  , _vbuf(std::move(vbuf))
  , _element_count(index_count)
{
}
