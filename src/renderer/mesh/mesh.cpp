#include "mesh/mesh.hpp"

mr::Mesh::Mesh(const VulkanState &state,
               std::span<PositionType> positions, std::span<FaceType> faces,
               std::span<ColorType> colors, std::span<TexCoordType> uvs,
               std::span<NormalType> normals, std::span<NormalType> tangents,
               std::span<NormalType> bitangents, std::span<BoneType> bones,
               BoundboxType bbox)
{
  std::vector<Vertex> vertices;
  vertices.reserve(positions.size());
  for (int i = 0; i < positions.size(); i++) {
    vertices.emplace_back(
      positions[i],
      ColorType{1, 0, 0, 1}, // colors[i],
      TexCoordType{}, // uvs[i],
      NormalType{}, // normals[i],
      NormalType{}, // tangents[i],
      NormalType{} // bitangents[i]
    );
  }

  std::vector<int> indices;
  indices.reserve(positions.size());
  for (int i = 0; i < faces.size(); i++) {
    indices.insert(indices.end(), faces[i].mIndices, faces[i].mIndices + faces[i].mNumIndices);
  }

  _element_count = indices.size();

  _vbuf = mr::VertexBuffer(state, std::span{vertices});
  _ibuf = mr::IndexBuffer(state, std::span{indices});
}
