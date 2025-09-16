#include "mesh/mesh.hpp"

mr::graphics::Mesh::Mesh(
	std::vector<VertexBuffer> vbufs,
	std::vector<IndexBuffer> ibufs,
  size_t instance_count,
  size_t mesh_offset,
  size_t instance_offset) noexcept
  : _ibufs(std::move(ibufs))
  , _vbufs(std::move(vbufs))
  , _instance_count(instance_count)
  , _mesh_offset(mesh_offset)
  , _instance_offset(instance_offset)
{
}
