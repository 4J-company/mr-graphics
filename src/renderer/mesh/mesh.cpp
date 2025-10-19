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

void mr::graphics::Mesh::bind(mr::CommandUnit &unit) const noexcept
{
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
