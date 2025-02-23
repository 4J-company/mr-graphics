#include "mesh/mesh.hpp"

/**
 * @brief Constructs a Mesh object.
 *
 * Initializes a Mesh by taking ownership of the provided vertex and index buffers using move semantics.
 * The provided vertex buffers encapsulate the mesh's vertex data and the index buffer defines its topology.
 * Additionally, the mesh's instance count is set to 1. This constructor is noexcept.
 *
 * @param vbufs A vector of VertexBuffer objects containing the mesh's vertex data.
 * @param ibuf An IndexBuffer object representing the mesh's index data.
 */
mr::Mesh::Mesh(
	std::vector<VertexBuffer> vbufs,
	IndexBuffer ibuf) noexcept
  : _ibuf(std::move(ibuf))
  , _vbufs(std::move(vbufs))
  , _instance_count(1)
{
}
