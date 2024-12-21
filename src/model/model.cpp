#include "model/model.hpp"
#include "renderer/renderer.hpp"
#include "utils/path.hpp"
#include "utils/log.hpp"

#define TINYGLTF_NO_INCLUDE_RAPIDJSON
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include "tiny_gltf.h"

static std::optional<mr::Texture> load_texture(const mr::VulkanState &state,
                                               const tinygltf::Model &image,
                                               const int index) noexcept;
static mr::Material load_material(const mr::VulkanState &state,
                                  vk::RenderPass render_pass,
                                  const tinygltf::Model &model,
                                  const tinygltf::Material &material,
                                  mr::FPSCamera &cam,
                                  mr::Matr4f transform) noexcept;
static mr::Matr4f calculate_transform(const tinygltf::Node &node,
                                      const mr::Matr4f &parent_transform) noexcept;

mr::Model::Model(const VulkanState &state, vk::RenderPass render_pass, std::string_view filename, mr::FPSCamera &cam) noexcept
{
  MR_INFO("Loading model {}", filename);

  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  std::filesystem::path model_path = path::models_dir / filename;

  bool ret;
  if (model_path.extension() == ".gltf") {
    ret = loader.LoadASCIIFromFile(&model, &err, &warn, model_path.string());
  }
  else {
    ret = loader.LoadBinaryFromFile(&model, &err, &warn, model_path.string());
  }
  if (!ret) {
    MR_ERROR("GLTF ERROR: {}", err);
    MR_WARNING("GLTF WARNING: {}", warn);
    exit(-1);
  }

  // only load first scene since we assume only 1 model is loaded
  const auto &scene = model.scenes[0];
  for (int i = 0; i < scene.nodes.size(); i++) {
    const auto &node = model.nodes[scene.nodes[i]];
    _process_node(state, render_pass, model, mr::Matr4f::identity(), cam, node);
  }

  MR_INFO("Loading model {} finished\n", filename);
}

void mr::Model::draw(CommandUnit &unit) const noexcept
{
  for (const auto &[material, mesh] : std::views::zip(_materials, _meshes)) {
    material.bind(unit);
    unit->bindVertexBuffers(0, mesh.vbuf().buffer(), {0});
    unit->bindIndexBuffer(mesh.ibuf().buffer(), 0, mesh.ibuf().index_type());
    unit->drawIndexed(mesh.ibuf().element_count(), mesh.num_of_instances(), 0, 0, 0);
  }
}

void mr::Model::_process_node(const mr::VulkanState &state,
                              const vk::RenderPass &render_pass,
                              tinygltf::Model &model,
                              const mr::Matr4f &parent_transform,
                              mr::FPSCamera &cam,
                              const tinygltf::Node &node) noexcept
{
  std::array attributes {
    "POSITION",
    "NORMAL",
    "TEXCOORD_0",
  };

  if (node.mesh < 0) {
    return;
  }

  auto &mesh = model.meshes[node.mesh];
  mr::Matr4f transform = calculate_transform(node, parent_transform);

  for (auto child : node.children) {
    _process_node(state, render_pass, model, transform, cam, model.nodes[child]);
  }

  for (auto &primitive : mesh.primitives) {
    struct Vertex {
        float position_x, position_y, position_z;
        float normal_x, normal_y, normal_z;
        float tex_coord_x, tex_coord_y;
    };

    std::vector<Vertex> vertexes;
    VertexBuffer vbuf;
    IndexBuffer ibuf;

    for (const char * attribute : attributes) {
      if (primitive.attributes.find(attribute) == primitive.attributes.end()) {
        continue;
      }

      const auto &accessor = model.accessors[primitive.attributes[attribute]];
      const auto &view = model.bufferViews[accessor.bufferView];
      const size_t attribute_size = std::strcmp(attribute, "POSITION") == 0     ? 3
                                    : std::strcmp(attribute, "NORMAL") == 0     ? 3
                                    : std::strcmp(attribute, "TEXCOORD_0") == 0 ? 2
                                                                                : 0;
      const float *attribute_buffer = (float *)&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
      const size_t attribute_count = accessor.count;

      // convert from AoS to SoA
      if (vertexes.size() == 0) {
        vertexes.resize(accessor.count);
      }

      if (std::strcmp(attribute, "POSITION") == 0) {
        for (int i = 0; i < vertexes.size(); i++) {
          vertexes[i].position_x = attribute_buffer[i * attribute_size + 0];
          vertexes[i].position_y = attribute_buffer[i * attribute_size + 1];
          vertexes[i].position_z = attribute_buffer[i * attribute_size + 2];
                                   1 ;
        }
      }
      else if (std::strcmp(attribute, "NORMAL") == 0) {
        for (int i = 0; i < vertexes.size(); i++) {
          vertexes[i].normal_x = attribute_buffer[i * attribute_size + 0];
          vertexes[i].normal_y = attribute_buffer[i * attribute_size + 1];
          vertexes[i].normal_z = attribute_buffer[i * attribute_size + 2];
                                 1 ;
        }
      }
      else if (std::strcmp(attribute, "TEXCOORD_0") == 0) {
        for (int i = 0; i < vertexes.size(); i++) {
          vertexes[i].tex_coord_x = attribute_buffer[i * attribute_size + 0];
          vertexes[i].tex_coord_y = attribute_buffer[i * attribute_size + 1];
                                    0;
                                    1;
        }
      }
    }

    vbuf = VertexBuffer(state, std::span {vertexes});

    const auto &accessor = model.accessors[primitive.indices];
    const auto &view = model.bufferViews[accessor.bufferView];
    const auto &buffer = model.buffers[view.buffer];
    size_t index_count = accessor.count;

    switch (accessor.componentType) {
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        {
          const uint32_t *index_buffer =
            (uint32_t *)&(buffer.data[accessor.byteOffset + view.byteOffset]);
          ibuf = IndexBuffer(state, std::span {index_buffer, index_count});
        }
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        {
          const uint16_t *index_buffer =
            (uint16_t *)&(buffer.data[accessor.byteOffset + view.byteOffset]);
          ibuf = IndexBuffer(state, std::span {index_buffer, index_count});
        }
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        {
          const uint8_t *index_buffer =
            (uint8_t *)&(buffer.data[accessor.byteOffset + view.byteOffset]);
          ibuf = IndexBuffer(state, std::span {index_buffer, index_count});
        }
        break;
      default:
        assert(0 && "that's crazy");
    }

    int material_index = primitive.material;
    tinygltf::Material material = model.materials[primitive.material];

    _meshes.emplace_back(std::move(vbuf), std::move(ibuf));
    _materials.emplace_back(load_material(state, render_pass, model, material, cam, transform));
  }
}

static std::optional<mr::Texture> load_texture(const mr::VulkanState &state,
                                               const tinygltf::Model &model,
                                               const int index) noexcept
{
  if (index < 0) { return std::nullopt; }

  const tinygltf::Texture &texture = model.textures[index];
  const tinygltf::Image   &image = model.images[texture.source];

  // NOTE: most devices dont support 3 component texels
  vk::Format format = image.component == 3 ? vk::Format::eR8G8B8Srgb : vk::Format::eR8G8B8A8Srgb;
  return mr::Texture(state, image.image.data(), image.height, image.width, format);
}

static mr::Vec4f load_constant(const std::vector<double> &constant) noexcept
{
  std::array<float, 4> tmp {};
  for (int i = 0; i < constant.size(); i++) {
    tmp[i] = constant[i];
  }
  return {
    tmp[0],
    tmp[1],
    tmp[2],
    tmp[3],
  };
}

static mr::Material load_material(
    const mr::VulkanState &state,
    vk::RenderPass render_pass,
    const tinygltf::Model &model,
    const tinygltf::Material &material,
    mr::FPSCamera &cam,
    mr::Matr4f transform) noexcept
{
  /*
    Blinn-Phong textures:
        - ambient
        - diffuse
        - specular
        - shininess
    PBR textures:
        - base color
        - roughness
        - metalness
        - emissive
        - occlusion
  */

  static std::vector<mr::MaterialBuilder> builders;

  mr::Vec4f ambient_color  {1, 1, 0, 1};
  mr::Vec4f diffuse_color  {0.8, 0.8, 0.8, 1};
  mr::Vec4f specular_color {0.1, 0.1, 0.1, 1};
  float shininess = 0.5;

  mr::Vec4f base_color_factor = load_constant(material.pbrMetallicRoughness.baseColorFactor);
  mr::Vec4f emissive_color_factor = load_constant(material.emissiveFactor);
  float metallic_factor = material.pbrMetallicRoughness.metallicFactor;
  float roughness_factor = material.pbrMetallicRoughness.roughnessFactor;

  std::optional<mr::Texture> base_color_texture_optional         = load_texture(state, model, material.pbrMetallicRoughness.baseColorTexture.index);
  std::optional<mr::Texture> metallic_roughness_texture_optional = load_texture(state, model, material.pbrMetallicRoughness.metallicRoughnessTexture.index);
  std::optional<mr::Texture> emissive_texture_optional           = load_texture(state, model, material.emissiveTexture.index);
  std::optional<mr::Texture> occlusion_texture_optional          = load_texture(state, model, material.occlusionTexture.index);
  std::optional<mr::Texture> normal_texture_optional             = load_texture(state, model, material.normalTexture.index);

  mr::MaterialBuilder builder = mr::MaterialBuilder(state, render_pass, "default");
  builder
    .add_value(transform)
    .add_texture("BASE_COLOR_MAP", std::move(base_color_texture_optional), base_color_factor)
    .add_texture("METALLIC_ROUGHNESS_MAP", std::move(metallic_roughness_texture_optional.value()))
    .add_texture("EMISSIVE_MAP", std::move(emissive_texture_optional), emissive_color_factor)
    .add_texture("OCCLUSION_MAP", std::move(occlusion_texture_optional))
    .add_texture("NORMAL_MAP", std::move(normal_texture_optional))
    .add_camera(cam);
  builders.emplace_back(std::move(builder));
  return builders.back().build();
}

static mr::Matr4f calculate_transform(const tinygltf::Node &node, const mr::Matr4f &parent_transform) noexcept {
  // calculate local transform
  mr::Matr4f transform = mr::Matr4f::identity();
  if (node.scale.size() == 3) {
    transform *= mr::Matr4f::scale(
      mr::Vec3f(
        node.scale[0],
        node.scale[1],
        node.scale[2]
        ));
  }
  if (node.rotation.size() == 4) {
    transform *= mr::Matr4f::rotate(mr::Norm3f(
                                        node.rotation[0],
                                        node.rotation[1],
                                        node.rotation[2]),
                                    mr::Radiansf(
                                        node.rotation[3])
    );
  }
  if (node.translation.size() == 3) {
    transform *= mr::Matr4f::translate(mr::Vec3f(
        node.translation[0],
        node.translation[1],
        node.translation[2]
    ));
  }
  if (node.matrix.size() == 16) {
    transform =
      mr::Matr4f(
          node.matrix[0], node.matrix[4], node.matrix[ 8], node.matrix[12],
          node.matrix[1], node.matrix[5], node.matrix[ 9], node.matrix[13],
          node.matrix[2], node.matrix[6], node.matrix[10], node.matrix[14],
          node.matrix[3], node.matrix[7], node.matrix[11], node.matrix[15]
      );
  }

  return transform * parent_transform;
}
