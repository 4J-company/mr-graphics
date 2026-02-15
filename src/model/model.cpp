#include "renderer/window/render_context.hpp"

#include "scene/scene.hpp"
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

  const auto &state = scene.render_context().vulkan_state();

  std::filesystem::path model_path = path::models_dir / filename;

  bool is_1component_supported = mr::TextureImage::is_texture_format_supported(state, vk::Format::eR8Uint);
  bool is_2component_supported = mr::TextureImage::is_texture_format_supported(state, vk::Format::eR8G8Uint);
  bool is_3component_supported = mr::TextureImage::is_texture_format_supported(state, vk::Format::eR8G8B8Uint);
  bool is_4component_supported = mr::TextureImage::is_texture_format_supported(state, vk::Format::eR8G8B8A8Uint);

  using enum mr::importer::Options;

  Options options {
    Options::All
    & (is_1component_supported ? Options::All : ~Options::Allow1ComponentImages)
    & (is_2component_supported ? Options::All : ~Options::Allow2ComponentImages)
    & (is_3component_supported ? Options::All : ~Options::Allow3ComponentImages)
    & (is_4component_supported ? Options::All : ~Options::Allow4ComponentImages)
    & ~Options::PreferUncompressed
  };

  auto model = mr::import(model_path, options);
  if (!model) {
    MR_ERROR("Loading model {} failed", model_path.string());
    return;
  }
  auto& model_value = model.value();

  CommandUnit geometry_command_unit (state);
  geometry_command_unit.begin();

  using enum mr::MaterialParameter;
  static auto &manager = ResourceManager<Texture>::get();
  std::for_each(std::execution::seq, model_value.meshes.begin(), model_value.meshes.end(),
    [&, this] (auto &mesh) {
      ASSERT(mesh.material < model_value.materials.size(), "Failed to load material from GLTF file");

      const auto &material = model_value.materials[mesh.material];
      const auto &transform = mesh.transforms[0];

      const size_t instance_count = mesh.transforms.size();
      const size_t instance_offset = scene._transforms_data.size();
      const size_t mesh_offset = scene._mesh_offset++;

      MR_DEBUG("{}: [{}; {})", mesh.name, instance_offset, instance_offset + instance_count);

      std::array vbufs_data {
        std::as_bytes(std::span(mesh.positions)),
        std::as_bytes(std::span(mesh.attributes))
      };
      auto vbufs = scene.render_context().add_vertex_buffers(geometry_command_unit, vbufs_data);

      std::vector<IndexBufferDescription> ibufs;
      ibufs.reserve(mesh.lods.size());
      for (size_t j = 0; j < mesh.lods.size(); j++) {
        ibufs.emplace_back(IndexBufferDescription {
          .offset = scene.render_context().index_buffer().allocate_and_write(geometry_command_unit, std::span(mesh.lods[j].indices)),
          .elements_count = static_cast<uint32_t>(mesh.lods[j].indices.size())
        });
      }

      // TODO(dk6): maybe erase this
      scene._visibility_data.emplace_back(1);

      uint32_t instance_render_info_size = sizeof(uint32_t);
      if (is_render_option_enabled(scene.render_context().options(), RenderOptions::EnableCullingVisualiztion)) {
        instance_render_info_size += sizeof(uint32_t);
      }

      auto &mesh_descr = _meshes.emplace_back(MeshInstances {
        .mesh = graphics::Mesh(std::move(vbufs), std::move(ibufs),
                                          instance_count, mesh_offset, instance_offset,
                                          mesh.aabb),
        .instances_number = static_cast<uint32_t>(instance_count),
        .transforms = std::move(mesh.transforms),
        // TODO(dk6): use dynamic buffer
        .intances_render_info_buffer = StorageBuffer(state, instance_render_info_size * Scene::max_scene_instances),
      });
      mesh_descr.intances_render_info_buffer_id =
        scene.render_context().bindless_set().register_resource(&mesh_descr.intances_render_info_buffer);

      if (is_render_option_enabled(scene.render_context().options(), RenderOptions::EnableCullingVisualiztion)) {
        // TODO(dk6): Try change uint int to byte
        // TODO(dk6): use dynamic buffer
        mesh_descr.occluded_instances_render_buffer = StorageBuffer(state, sizeof(uint32_t) * Scene::max_scene_instances);
        mesh_descr.occluded_instances_render_buffer_id =
          scene.render_context().bindless_set().register_resource(&mesh_descr.occluded_instances_render_buffer);
      }

      mr::MaterialBuilder builder(scene, "default");

      builder.add_camera(scene.camera_uniform_buffer());
      builder.add_value(&material.constants);
      for (const auto &texture : material.textures) {
        builder.add_texture(importer2graphics(texture.type), texture);
      }
      builder.add_storage_buffer(&scene._transforms);
      builder.add_conditional_buffer(&scene._visibility);

      _builders.push_back(std::move(builder));
      _materials.push_back(_builders.back().build());
    }
  );
  geometry_command_unit.end();

  UniqueFenceGuard(
    scene.render_context().vulkan_state().device(),
    geometry_command_unit.submit(scene.render_context().vulkan_state())
  );

  MR_INFO("Loading model {} finished\n", filename.string());
}

mr::Matr4f mr::graphics::Model::transform(uint32_t instance) const noexcept
{
  ASSERT(instance < _transforms_data.size());
  return _transforms_data[instance];
}
