#include "mesh/mesh.hpp"
#include <vulkan/vulkan_core.h>

mr::graphics::Mesh::Mesh(VertexBuffersArray vbufs,
                         std::vector<IndexBufferDescription> ibufs,
                         size_t instance_count,
                         size_t mesh_offset,
                         size_t instance_offset,
                         const AABBf &bound_box,
                         std::vector<Matr4f> transforms) noexcept
  : _vbufs(std::move(vbufs))
  , _ibufs(std::move(ibufs))
  , _instance_count(instance_count)
  , _mesh_offset(mesh_offset)
  , _instance_offset(instance_offset)
  , _bound_box(bound_box)
  , _base_transforms(std::move(transforms))
{
}
