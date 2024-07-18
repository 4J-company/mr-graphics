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

  // load meshes
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

  // load textures
  if (sc->mTextures != nullptr) {
    std::transform(
        sc->mTextures,
        sc->mTextures + sc->mNumTextures,
        _textures.begin(),
        [&state](aiTexture *t) -> mr::Texture {
          // file in a compressed file format
          if (t->mHeight == 0) {
            return Texture(state, t->mFilename.C_Str());
          }
          // NOTE: hard-coded vk::Format is specified by the assimp
          return Texture(state, reinterpret_cast<byte *>(t->pcData), t->mHeight, t->mWidth, vk::Format::eR8G8B8A8Srgb);
        });
  }
}

void mr::Model::draw(CommandUnit &unit) const noexcept {
  for (const auto &mesh : _meshes) {
    unit->bindVertexBuffers(0, {mesh.vbuf().buffer()}, {0});
    unit->bindIndexBuffer(mesh.ibuf().buffer(), 0, vk::IndexType::eUint32);
    unit->drawIndexed(mesh.num_of_indices(), 1, 0, 0, 0);
  }
}
