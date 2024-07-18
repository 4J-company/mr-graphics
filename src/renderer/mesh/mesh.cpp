#include "mesh/mesh.hpp"
#include "mesh/attribute_types.hpp"

mr::Mesh::Mesh(const VulkanState &state,
               std::span<PositionType> positions, std::span<FaceType> faces,
               std::span<ColorType> colors, std::span<TexCoordType> uvs,
               std::span<NormalType> normals, std::span<NormalType> tangents,
               std::span<NormalType> bitangents, std::span<BoneType> bones,
               BoundboxType bbox)
{
  std::vector<Vertex> vertices;
  vertices.resize(positions.size());
  for (int i = 0; i < positions.size(); i++) {
    vertices[i] = {
      positions[i],
      colors.data() == nullptr ? ColorType {} : colors[i],
      uvs.data() == nullptr ? TexCoordType {} : uvs[i],
      normals.data() == nullptr ? NormalType {} : normals[i],
      tangents.data() == nullptr ? NormalType {} : tangents[i],
      bitangents.data() == nullptr ? NormalType {} : bitangents[i]
    };
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
