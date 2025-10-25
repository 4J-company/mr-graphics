#include "mesh/mesh.hpp"
#include <vulkan/vulkan_core.h>

mr::graphics::Mesh::Mesh(std::vector<VkDeviceSize> vbufs,
                         std::vector<IndexBufferDescription> ibufs,
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
