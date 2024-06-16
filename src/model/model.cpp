module;
#include "pch.hpp"
export module Model;

import Renderer;

export namespace mr {
  class Model {
    private:
      std::vector<mr::Mesh *> _meshes;
      std::string _name;

    public:
      Model() = default;
      Model(std::string_view filename, const Application &app) noexcept;
      Model(const Model &other) noexcept = default;
      Model &operator=(const Model &other) noexcept = default;
      Model(Model &&other) noexcept = default;
      Model &operator=(Model &&other) noexcept = default;
  };
} // namespace mr

mr::Model::Model(std::string_view filename, const mr::Application &app) noexcept
{
  Assimp::Importer importer;
  const aiScene *sc =
    importer.ReadFile(std::filesystem::current_path()
                        .append("build")
                        .append("models")
                        .append(filename)
                        .string(),
                      aiProcessPreset_TargetRealtime_MaxQuality);

  assert(sc != nullptr);
  assert(sc->mMeshes != nullptr);

  _meshes.resize(sc->mNumMeshes);
  std::transform(// std::execution::seq,
                 sc->mMeshes,
                 sc->mMeshes + sc->mNumMeshes,
                 _meshes.begin(),
                 [&app](aiMesh *m) -> mr::Mesh * {
                   return app.create_mesh({m->mVertices, m->mNumVertices},
                                          {m->mFaces, m->mNumFaces},
                                          {},
                                          {},
                                          {},
                                          {},
                                          {},
                                          {},
                                          {});
                 });
}
