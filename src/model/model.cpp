#include "model/model.hpp"

#include "scene/scene.hpp"
#include "renderer/window/render_context.hpp"

constexpr mr::MaterialParameter importer2graphics(mr::importer::TextureType type)
{
  switch (type) {
    case mr::importer::TextureType::BaseColor:
      return mr::graphics::MaterialParameter::BaseColor;
    case mr::importer::TextureType::RoughnessMetallic:
      return mr::graphics::MaterialParameter::MetallicRoughness;
    case mr::importer::TextureType::OcclusionRoughnessMetallic:
      return mr::graphics::MaterialParameter::MetallicRoughness;
    case mr::importer::TextureType::EmissiveColor:
      return mr::graphics::MaterialParameter::EmissiveColor;
    case mr::importer::TextureType::NormalMap:
      return mr::graphics::MaterialParameter::NormalMap;
    case mr::importer::TextureType::OcclusionMap:
      return mr::graphics::MaterialParameter::OcclusionMap;
    default:
      ASSERT(false, "Unhandled mr::importer::TextureType", type);
      return mr::graphics::MaterialParameter::EnumSize;
  }
}

mr::graphics::Model::Model(
    const mr::graphics::Scene &scene,
    std::fs::path filename) noexcept
{
  MR_INFO("Loading model {}", filename.string());

  const auto &state = scene.render_context().vulkan_state();

  std::filesystem::path model_path = path::models_dir / filename;

  auto model = mr::import(model_path);
  if (!model) {
    MR_ERROR("Loading model {} failed", model_path.string());
    return;
  }
  auto& model_value = model.value();

  auto defult_shader_path = mr::path::shaders_dir / "default";
  auto defult_shader_path_str = defult_shader_path.string();

  using enum mr::MaterialParameter;
  static std::vector<mr::MaterialBuilder> builders;
  static auto &manager = ResourceManager<Texture>::get();
  auto io = std::ranges::iota_view {(size_t)0, model_value.meshes.size()};
  std::for_each(std::execution::seq, io.begin(), io.end(),
    [&, this] (size_t i) {
      const auto &mesh = model_value.meshes[i];

      ASSERT(mesh.material < model_value.materials.size(), "Failed to load material from GLTF file");

      const auto &material = model_value.materials[mesh.material];
      const auto &transform = mesh.transforms[0];

      std::vector<VertexBuffer> vbufs;
      vbufs.emplace_back(state, std::span(mesh.positions.data(), mesh.positions.size()));
      vbufs.emplace_back(state, std::span(mesh.attributes.data(), mesh.attributes.size()));
      IndexBuffer ibuf {state, std::span(mesh.lods[0].indices.data(), mesh.lods[0].indices.size())};

      _meshes.emplace_back(std::move(vbufs), std::move(ibuf));

      mr::MaterialBuilder builder {state, scene.render_context(), filename.stem().string()};

      ASSERT(s_cam_ubo_ptr);

      builder.add_camera(*s_cam_ubo_ptr);
      builder.add_value(&material.constants);
      for (const auto &texture : material.textures) {
        builder.add_texture(importer2graphics(texture.type), texture);
      }
      builders.push_back(std::move(builder));
      _materials.push_back(builders.back().build());
    }
  );

  MR_INFO("Loading model {} finished\n", filename.string());
}

void mr::graphics::Model::draw(CommandUnit &unit) const noexcept
{
  for (const auto &[material, mesh] : std::views::zip(_materials, _meshes)) {
    material->bind(unit);
    mesh.bind(unit);
    mesh.draw(unit);
  }
}

