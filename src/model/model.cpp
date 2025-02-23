#include "model/model.hpp"
#include "renderer/renderer.hpp"
#include "utils/path.hpp"
#include "utils/log.hpp"

// for tmp singleton
#include "manager/manager.hpp"

#define TINYGLTF_NO_INCLUDE_RAPIDJSON
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include "tiny_gltf.h"

static std::optional<mr::TextureHandle> load_texture(const mr::VulkanState &state,
                                                     const tinygltf::Model &image,
                                                     const int index) noexcept;
static mr::MaterialHandle load_material(const mr::VulkanState &state,
                                        vk::RenderPass render_pass,
                                        const tinygltf::Model &model,
                                        const tinygltf::Material &material,
                                        mr::UniformBuffer &cam_ubo,
                                        mr::Matr4f transform) noexcept;
static mr::Matr4f calculate_transform(const tinygltf::Node &node,
                                      const mr::Matr4f &parent_transform) noexcept;

/**
 * @brief Loads and initializes a 3D model from a GLTF file.
 *
 * This constructor loads a GLTF model using TinyGLTF by selecting ASCII or binary loading based on the file extension.
 * It processes the first scene's nodes recursively to create meshes and materials for Vulkan rendering. If the model
 * fails to load, it logs the error and warning messages before terminating the program.
 *
 * @param filename The GLTF model file name relative to the models directory.
 */
mr::Model::Model(const VulkanState &state, vk::RenderPass render_pass, std::string_view filename, mr::UniformBuffer &cam_ubo) noexcept
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
    _process_node(state, render_pass, model, mr::Matr4f::identity(), cam_ubo, node);
  }

  MR_INFO("Loading model {} finished\n", filename);
}

/**
 * @brief Renders the model by drawing each mesh with its corresponding material.
 *
 * Iterates over the material and mesh pairs, binding each material and issuing draw commands for the associated mesh using the provided command unit.
 *
 * @param unit The command unit used to record drawing commands.
 */
void mr::Model::draw(CommandUnit &unit) const noexcept
{
  for (const auto &[material, mesh] : std::views::zip(_materials, _meshes)) {
    material->bind(unit);
    mesh.bind(unit);
    mesh.draw(unit);
  }
}

/**
 * @brief Recursively processes a GLTF node to extract mesh primitives and load associated materials.
 *
 * This function computes the node's transformation matrix by combining its local transform with the
 * provided parent transformation. If the node references a valid mesh (i.e., node.mesh is non-negative),
 * it iterates through each mesh primitive to extract supported vertex attributes (POSITION, NORMAL, TEXCOORD_0)
 * and creates corresponding vertex and index buffers. The material for each primitive is then loaded and both
 * the mesh and material are stored internally for later rendering.
 *
 * The function also recursively processes all child nodes using the computed transformation.
 *
 * @note If the node does not reference a mesh (i.e., node.mesh is negative), the function returns immediately.
 *
 * @param model The GLTF model containing mesh, buffer, and accessor data.
 * @param parent_transform The transformation matrix inherited from the parent node.
 * @param node The current GLTF node to process.
 */
void mr::Model::_process_node(const mr::VulkanState &state,
                              const vk::RenderPass &render_pass,
                              tinygltf::Model &model,
                              const mr::Matr4f &parent_transform,
                              mr::UniformBuffer &cam_ubo,
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
    _process_node(state, render_pass, model, transform, cam_ubo, model.nodes[child]);
  }

  for (auto &primitive : mesh.primitives) {
    std::vector<VertexBuffer> vbufs;
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

      vbufs.emplace_back(state, std::span {attribute_buffer, accessor.count * attribute_size});
    }

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

    _meshes.emplace_back(std::move(vbufs), std::move(ibuf));
    _materials.emplace_back(load_material(state, render_pass, model, material, cam_ubo, transform));
  }
}

/**
 * @brief Loads a texture from a GLTF model.
 *
 * This function attempts to load a texture specified by the given index from the GLTF model. If
 * the index is negative, std::nullopt is returned. Otherwise, it retrieves the corresponding texture and
 * associated image data, determines the appropriate Vulkan format based on the number of image components
 * (using a 3-component format if necessary, though most devices require 4 components), and creates a texture
 * using the shared resource manager.
 *
 * @param state Vulkan state used during texture creation.
 * @param model GLTF model containing texture and image data.
 * @param index Index of the texture to load.
 * @return std::optional<mr::TextureHandle> Handle to the created texture on success, or std::nullopt if the index is invalid.
 */
static std::optional<mr::TextureHandle> load_texture(const mr::VulkanState &state,
                                               const tinygltf::Model &model,
                                               const int index) noexcept
{
  if (index < 0) { return std::nullopt; }

  const tinygltf::Texture &texture = model.textures[index];
  const tinygltf::Image   &image = model.images[texture.source];

  mr::Extent extent ={static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height)};
  // NOTE: most devices dont support 3 component texels
  vk::Format format = image.component == 3 ? vk::Format::eR8G8B8Srgb : vk::Format::eR8G8B8A8Srgb;

  static auto &manager = mr::ResourceManager<mr::Texture>::get();
  return manager.create(mr::unnamed, state, image.image.data(), extent, format);
}

/**
 * @brief Creates a material for physically-based rendering from GLTF material properties.
 *
 * Constructs a material by extracting PBR parameters—such as base color, metallic-roughness, emissive, occlusion,
 * and normal textures—from the provided GLTF material. It loads available textures from the GLTF model, applies
 * associated color factors, and incorporates a transformation matrix into the material. A camera uniform is also
 * attached to support rendering.
 *
 * @param model GLTF model supplying image data for texture lookup.
 * @param material GLTF material descriptor specifying PBR properties and texture indices.
 * @param transform Transformation matrix applied to the material.
 *
 * @return mr::MaterialHandle Handle to the constructed material.
 *
 * @note Parameters like Vulkan state, render pass, and camera uniform buffer are common services and are omitted.
 *       Material builders are stored in a static collection across function invocations.
 */
static mr::MaterialHandle load_material(
    const mr::VulkanState &state,
    vk::RenderPass render_pass,
    const tinygltf::Model &model,
    const tinygltf::Material &material,
    mr::UniformBuffer &cam_ubo,
    mr::Matr4f transform) noexcept
{
  /*
    PBR textures:
        - base color
        - roughness
        - metalness
        - emissive
        - occlusion
  */

  static std::vector<mr::MaterialBuilder> builders;

  mr::Vec4f base_color_factor = std::span(material.pbrMetallicRoughness.baseColorFactor);
  mr::Vec4f emissive_color_factor = std::span(material.emissiveFactor);
  float metallic_factor = material.pbrMetallicRoughness.metallicFactor;
  float roughness_factor = material.pbrMetallicRoughness.roughnessFactor;

  std::optional<mr::TextureHandle> base_color_texture_optional         = load_texture(state, model, material.pbrMetallicRoughness.baseColorTexture.index);
  std::optional<mr::TextureHandle> metallic_roughness_texture_optional = load_texture(state, model, material.pbrMetallicRoughness.metallicRoughnessTexture.index);
  std::optional<mr::TextureHandle> emissive_texture_optional           = load_texture(state, model, material.emissiveTexture.index);
  std::optional<mr::TextureHandle> occlusion_texture_optional          = load_texture(state, model, material.occlusionTexture.index);
  std::optional<mr::TextureHandle> normal_texture_optional             = load_texture(state, model, material.normalTexture.index);

  using enum mr::MaterialParameter;
  mr::MaterialBuilder builder = mr::MaterialBuilder(state, render_pass, "default");
  builder
    .add_value(transform)
    .add_texture(BaseColor, std::move(base_color_texture_optional), base_color_factor)
    .add_texture(MetallicRoughness, std::move(metallic_roughness_texture_optional))
    .add_texture(EmissiveColor, std::move(emissive_texture_optional), emissive_color_factor)
    .add_texture(OcclusionMap, std::move(occlusion_texture_optional))
    .add_texture(NormalMap, std::move(normal_texture_optional))
    .add_camera(cam_ubo);
  builders.emplace_back(std::move(builder));
  return builders.back().build();
}

/**
 * @brief Computes the final transformation matrix for a GLTF node.
 *
 * Calculates the node's local transformation using its scale, rotation, and translation properties.
 * If an explicit 4x4 transformation matrix is provided within the node, it replaces the computed
 * local transform. The resulting matrix is then combined with the parent's transformation to produce
 * the final transform.
 *
 * @param node The GLTF node containing transformation data.
 * @param parent_transform The transformation matrix of the parent node.
 * @return mr::Matr4f The combined transformation matrix.
 */
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
