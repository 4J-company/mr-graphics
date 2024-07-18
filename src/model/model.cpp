#include "model/model.hpp"
#include "renderer/renderer.hpp"

mr::Model::Model(const VulkanState &state, std::string_view filename) noexcept
{
  Assimp::Importer importer;
  const aiScene *sc =
    importer.ReadFile(std::filesystem::current_path()
                        .append("bin")
                        .append("models")
                        .append(filename)
                        .string(),
                      aiProcessPreset_TargetRealtime_MaxQuality);

  assert(sc != nullptr);
  assert(sc->mMeshes != nullptr);

  _meshes.resize(sc->mNumMeshes);
  std::transform(sc->mMeshes,
                 sc->mMeshes + sc->mNumMeshes,
                 _meshes.begin(),
                 [&state](aiMesh *m) -> mr::Mesh {
                   // TODO: Investigate 'mColors', 'mTextureCoords', 'mBones'
                   return Mesh(state,
                               {m->mVertices, m->mNumVertices},
                               {m->mFaces, m->mNumFaces},
                               {m->mColors[0], m->mNumVertices},
                               {m->mTextureCoords[0], m->mNumVertices},
                               {m->mNormals, m->mNumVertices},
                               {m->mTangents, m->mNumVertices},
                               {m->mBitangents, m->mNumVertices},
                               {},
                               {});
                 });
}

void mr::Model::draw(CommandUnit &unit) const noexcept {
  for (const auto &mesh : _meshes) {
    unit->bindVertexBuffers(0, {mesh.vbuf().buffer()}, {0});
    unit->bindIndexBuffer(mesh.ibuf().buffer(), 0, vk::IndexType::eUint32);
    unit->drawIndexed(mesh.num_of_indices(), 1, 0, 0, 0);
  }
}
