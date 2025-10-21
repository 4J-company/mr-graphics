#include "mesh/mesh.hpp"

mr::graphics::Mesh::Mesh(
#if USE_MERGED_VERTEX_BUFFER
  std::vector<uint32_t> vbufs,
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
  size_t instance_offset) noexcept
  : _vbufs(std::move(vbufs))
  , _ibufs(std::move(ibufs))
  , _instance_count(instance_count)
  , _mesh_offset(mesh_offset)
  , _instance_offset(instance_offset)
{
}

void mr::graphics::Mesh::bind(mr::CommandUnit &unit) const noexcept
{
#if !USE_MERGED_VERTEX_BUFFER
 static std::array<vk::Buffer, 16> vbufs {};
 static std::array<vk::DeviceSize, 16> offsets {};

 for (int i = 0; i < _vbufs.size(); i++) {
   vbufs[i] = _vbufs[i].buffer();
 }

 unit->bindVertexBuffers(0,
     std::span{vbufs.data(), _vbufs.size()},
     std::span{offsets.data(), _vbufs.size()});
#endif

#if !USE_MERGED_INDEX_BUFFER
  unit->bindIndexBuffer(_ibufs[0].buffer(), 0, _ibufs[0].index_type());
#endif // !USE_MERGED_INDEX_BUFFER
}
