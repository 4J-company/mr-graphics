#include "model/model.hpp"
#include "renderer/renderer.hpp"

static mr::Material handle_material(const mr::VulkanState &state, aiMaterial *mat) noexcept
{
  mr::Material res;

  auto load_by_texture_type =
    [&state, mat](aiTextureType type) -> std::optional<std::vector<mr::Texture>> {
      int tex_count = mat->GetTextureCount(aiTextureType_AMBIENT);
      if (tex_count == 0) {
        return std::nullopt;
      }

      std::vector<mr::Texture> textures(tex_count);
      for (int i = 0; i < tex_count; i++) {
        aiString path;
        mat->GetTexture(aiTextureType_AMBIENT, i, &path);
        textures[i] = mr::Texture(state, path.C_Str());
      }

      return std::move(textures);
    };

  /* load Blinn-Phong related textures */
  auto ambient_textures  = load_by_texture_type(aiTextureType_AMBIENT);   // or AI_MATKEY_COLOR_AMBIENT
  auto diffuse_textures  = load_by_texture_type(aiTextureType_DIFFUSE);   // or AI_MATKEY_COLOR_DIFFUSE
  auto specular_textures = load_by_texture_type(aiTextureType_SPECULAR);  // or AI_MATKEY_COLOR_SPECULAR

  aiVector3D ambient_color;
  aiVector3D diffuse_color;
  aiVector3D specular_color;

  mat->Get(AI_MATKEY_COLOR_AMBIENT, ambient_color);
  mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse_color);
  mat->Get(AI_MATKEY_COLOR_SPECULAR, specular_color);

  if (/* valid Blinn-Phong textures */ true) {
    /* return Blinn-Phong type material */
    // TODO: use constant values if no texture was specified
    //       (or PBR if no constants was specified either)
    if (!ambient_textures.has_value()) return mr::Material();
    if (!diffuse_textures.has_value()) return mr::Material();
    if (!specular_textures.has_value()) return mr::Material();

    return mr::Material(
        std::move(ambient_textures.value()),
        std::move(diffuse_textures.value()),
        std::move(specular_textures.value())
    );
  }

  /* load PBR related textures */
  auto emissive_textures  = load_by_texture_type(aiTextureType_EMISSIVE);  // or AI_MATKEY_COLOR_EMISSIVE
  auto shininess_textures = load_by_texture_type(aiTextureType_SHININESS); // or AI_MATKEY_SHININESS
  auto opacity_textures   = load_by_texture_type(aiTextureType_OPACITY);   // or AI_MATKEY_OPACITY
  auto metalness_textures = load_by_texture_type(aiTextureType_METALNESS); //

  /* return PBR type material */

  return res;
}

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
  /* TODO: actually use this code and load textures to a manager
  if (sc->HasTextures()) {
    _textures.resize(sc->mNumTextures);
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
  */

  if (sc->HasMaterials()) {
    _materials.resize(sc->mNumMaterials);
    std::transform(sc->mMaterials,
                   sc->mMaterials + sc->mNumMaterials,
                   _materials.begin(),
                   [&state](aiMaterial *m) -> mr::Material {
                     return handle_material(state, m);
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
