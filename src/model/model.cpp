#include "model/model.hpp"

mr::Model::Model(std::string_view filename, const mr::Application &app) noexcept {
  Assimp::Importer importer;
  const aiScene *sc = importer.ReadFile(
      std::filesystem::current_path()
        .append("build")
        .append("models")
        .append(filename)
        .string(),
      aiProcessPreset_TargetRealtime_MaxQuality);

  assert(sc != nullptr);
  assert(sc->mMeshes != nullptr);

  _meshes.resize(sc->mNumMeshes);
  std::transform(std::execution::seq, sc->mMeshes, sc->mMeshes + sc->mNumMeshes, _meshes.begin(),
      [&app] (aiMesh *m) -> mr::Mesh * {
      return app.create_mesh(
          {m->mVertices, m->mNumVertices},
          {m->mFaces, m->mNumFaces},
          {}, {}, {}, {}, {}, {}, {});
      });
}
