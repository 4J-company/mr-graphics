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
    & ~Options::GenerateDiscreteLODs & ~Options::OptimizeMeshes
  };

  auto model = mr::import(model_path, options);
  if (!model) {
    MR_ERROR("Loading model {} failed", model_path.string());
    return;
  }
  auto& model_value = model.value();

  CommandUnit geometry_command_unit (state);
  geometry_command_unit.begin();

  // TODO(dk6): pass it as option
  constexpr bool split_on_meshlets = false;

  using enum mr::MaterialParameter;
  static auto &manager = ResourceManager<Texture>::get();
  std::for_each(std::execution::seq, model_value.meshes.begin(), model_value.meshes.end(),
    [&, this] (auto &mesh) {
      const auto &transform = mesh.transforms[0];

      const size_t instance_count = mesh.transforms.size();
      const size_t instance_offset = scene._transforms_data.size();
      const size_t mesh_offset = scene._mesh_offset++;

      MR_DEBUG("{}: [{}; {})", mesh.name, instance_offset, instance_offset + instance_count);

      mr::MaterialBuilder builder(scene, "default");
      builder.add_storage_buffer(&scene._transforms);
      builder.add_conditional_buffer(&scene._visibility);
      builder.add_camera(scene.camera_uniform_buffer());
      if (mesh.material < model_value.materials.size()) {
        const auto &material = model_value.materials[mesh.material];
        builder.add_value(&material.constants);
        for (const auto &texture : material.textures) {
          builder.add_texture(importer2graphics(texture.type), texture);
        }
      } else {
        mr::importer::MaterialData::ConstantBlock constant_block {
          .base_color_factor = Color(Vec4f{1}),
          .emissive_color = Color(Vec4f{1}),
          .emissive_strength = 1,
          .normal_map_intensity = 1,
          .roughness_factor = 1,
          .metallic_factor = 1,
        };
        builder.add_value(&constant_block);
      }

      _builders.push_back(std::move(builder));
      auto gpu_mtl = _builders.back().build();

      if (split_on_meshlets) {
        auto &lod = mesh.lods[0];
        for (auto &&[meshlet, bs] : std::views::zip(lod.meshlet_array.meshlets, lod.meshlet_bounds.bounding_spheres)) {
          const auto &meshlet_vertices = lod.meshlet_array.meshlet_vertices;
          const auto &meshlet_triangles = lod.meshlet_array.meshlet_triangles;

          struct TemporaryMesh {
            std::vector<importer::PackedVec3f> positions;
            std::vector<importer::VertexAttributes> attributes;
            std::vector<std::array<uint32_t, 3>> indices; // correct name is triangles
          };
          TemporaryMesh result;

          result.positions.reserve(meshlet.vertex_count);
          for (uint32_t i = 0; i < meshlet.vertex_count; ++i) {
            uint32_t vertex_index = meshlet_vertices[meshlet.vertex_offset + i];
            result.positions.push_back(mesh.positions[vertex_index]);
            result.attributes.push_back(mesh.attributes[vertex_index]);
          }

          result.indices.reserve(meshlet.triangle_count);
          for (uint32_t i = 0; i < meshlet.triangle_count; i++) {
            uint32_t triangle_index = meshlet.triangle_offset + i * 3;

            uint8_t v0 = meshlet_triangles[triangle_index + 0];
            uint8_t v1 = meshlet_triangles[triangle_index + 1];
            uint8_t v2 = meshlet_triangles[triangle_index + 2];

            result.indices.push_back({v0, v1, v2});
          }
          std::array vbufs_data {
            std::as_bytes(std::span(result.positions)),
            std::as_bytes(std::span(result.attributes))
          };
          auto vbufs = scene.render_context().add_vertex_buffers(geometry_command_unit, vbufs_data);

          IndexBufferDescription ibuf {
            .offset = scene.render_context().index_buffer().allocate_and_write(geometry_command_unit, std::span(result.indices)),
            .elements_count = static_cast<uint32_t>(result.indices.size() * 3)
          };
          std::vector<IndexBufferDescription> ibufs {ibuf};

          float r = bs.radius();
          auto c = bs.center();
          auto a = Vec3f(r, r, r);
          mr::AABBf bb {
            .min = c - a,
            .max = c + a,
          };

          bb.min = bb.max = Vec3f(result.positions[0][0], result.positions[0][1], result.positions[0][2]);
          for (const auto &p : result.positions) {
            bb.min.x(std::min(bb.min.x(), p[0]));
            bb.min.y(std::min(bb.min.y(), p[1]));
            bb.min.z(std::min(bb.min.z(), p[2]));
            bb.max.x(std::max(bb.max.x(), p[0]));
            bb.max.y(std::max(bb.max.y(), p[1]));
            bb.max.z(std::max(bb.max.z(), p[2]));
          }

          auto &mesh_descr = _meshes.emplace_back(MeshInstances {
            .mesh = graphics::Mesh(std::move(vbufs), std::move(ibufs), instance_count,
                                   mesh_offset, instance_offset, bb, bs),
            .instances_number = static_cast<uint32_t>(instance_count),
            .transforms = mesh.transforms,
            // TODO(dk6): use dynamic buffer
            .intances_render_info_buffer = StorageBuffer(state, sizeof(uint32_t) * 10),
          });
          mesh_descr.intances_render_info_buffer_id =
            scene.render_context().bindless_set().register_resource(&mesh_descr.intances_render_info_buffer);

          _materials.push_back(gpu_mtl);
        }
      } else {
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

        auto &mesh_descr = _meshes.emplace_back(MeshInstances {
          .mesh = graphics::Mesh(std::move(vbufs), std::move(ibufs),
                                 instance_count, mesh_offset, instance_offset,
                                 mesh.aabb, mesh.bounding_sphere),
          .instances_number = static_cast<uint32_t>(instance_count),
          .transforms = std::move(mesh.transforms),
          // TODO(dk6): use dynamic buffer
          .intances_render_info_buffer = StorageBuffer(state, instance_render_info_size * 100'000),
        });
        mesh_descr.intances_render_info_buffer_id =
          scene.render_context().bindless_set().register_resource(&mesh_descr.intances_render_info_buffer);

        _materials.push_back(gpu_mtl);
      }
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
