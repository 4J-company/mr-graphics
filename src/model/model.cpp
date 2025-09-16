#include "renderer/window/render_context.hpp"

#include "model/model.hpp"

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
    Scene &scene,
    std::fs::path filename) noexcept
  : _scene(&scene)
{
  MR_INFO("Loading model {}", filename.string());

  std::filesystem::path model_path = path::models_dir / filename;

  auto model = mr::import(model_path);
  if (!model) {
    MR_ERROR("Loading model {} failed", model_path.string());
    return;
  }
  auto& model_value = model.value();

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

      const size_t instance_count = mesh.transforms.size();
      const size_t instance_offset = scene._transforms_data.size();
      const size_t mesh_offset = scene._bounds_data.size();

      std::vector<VertexBuffer> vbufs;
      vbufs.reserve(2);
      vbufs.emplace_back(scene.render_context().vulkan_state(),
          std::span(mesh.positions.data(), mesh.positions.size()));
      vbufs.emplace_back(scene.render_context().vulkan_state(),
          std::span(mesh.attributes.data(), mesh.attributes.size()));

      std::vector<IndexBuffer> ibufs;
      vbufs.reserve(mesh.lods.size());
      for (size_t j = 0; j < mesh.lods.size(); j++) {
        ibufs.emplace_back(scene.render_context().vulkan_state(),
          std::span(mesh.lods[j].indices.data(), mesh.lods[j].indices.size())
        );
      }

      scene._bounds_data.emplace_back();
      scene._visibility_data.emplace_back(1);
      scene._transforms_data.insert(
        scene._transforms_data.end(),
        mesh.transforms.begin(),
        mesh.transforms.end()
      );

      _meshes.emplace_back(
        std::move(vbufs),
        std::move(ibufs),
        instance_count,
        mesh_offset,
        instance_offset
      );

      mr::MaterialBuilder builder {
        scene.render_context().vulkan_state(),
        scene.render_context(),
        "default"
      };

      builder.add_camera(scene.camera_uniform_buffer());
      builder.add_value(&material.constants);
      for (const auto &texture : material.textures) {
        builder.add_texture(importer2graphics(texture.type), texture);
      }
      builder.add_storage_buffer(&scene._transforms);
      builder.add_storage_buffer(&scene._bounds);
      builder.add_conditional_buffer(&scene._visibility);

      builders.push_back(std::move(builder));
      _materials.push_back(builders.back().build());
    }
  );

  MR_INFO("Loading model {} finished\n", filename.string());
}

void mr::graphics::Model::draw(CommandUnit &unit) const noexcept
{
  ASSERT(_scene != nullptr, "Parent scene of the model is lost");
  for (const auto &[material, mesh] : std::views::zip(_materials, _meshes)) {
    uint32_t offsets[2] {
      mesh._mesh_offset,
      mesh._instance_offset,
    };

    vk::ConditionalRenderingBeginInfoEXT conditional_rendering_begin_info {
      .buffer = _scene->_visibility.buffer(),
      .offset = mesh._mesh_offset,
    };

    _scene->render_context().vulkan_state().disp().cmdBeginConditionalRenderingEXT(
      unit.command_buffer(),
      conditional_rendering_begin_info
    );
    material->bind(unit);
    unit->pushConstants(material->pipeline().layout(), vk::ShaderStageFlagBits::eFragment, 0, 8, offsets);
    mesh.bind(unit);
    mesh.draw(unit);
    _scene->render_context().vulkan_state().disp().cmdEndConditionalRenderingEXT(unit.command_buffer());
  }
}

