#include "model/model.hpp"
#include "renderer/renderer.hpp"

#define TINYGLTF_NO_INCLUDE_RAPIDJSON
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include "tiny_gltf.h"

static mr::Material handle_material(const mr::VulkanState &state, vk::RenderPass render_pass) noexcept
{
  /*
    NOTE:
    Blinn-Phong textures:
        - ambient
        - diffuse
        - specular
        - shininess
    PBR textures:
        - base color
        - roughness
        - metalness
  */

  mr::Vec3f ambient_color  {1, 0, 0};
  mr::Vec3f diffuse_color  {0, 1, 0};
  mr::Vec3f specular_color {0, 0, 1};
  float shininess = 0.5;
  mr::Matr4f transform {
    -1.41421354,
    0.816496611,
    -0.578506112,
    -0.577350259,
    0.00000000,
    -1.63299322,
    -0.578506112,
    -0.577350259,
    1.41421354,
    0.816496611,
    -0.578506112,
    -0.577350259,
    0.00000000,
    0.00000000,
    34.5101662,
    34.6410141,
  };

  return mr::Material(
    state,
    render_pass,
    mr::Shader(state, "default"),
    ambient_color,
    diffuse_color,
    specular_color,
    shininess,
    transform
    );
}

mr::Model::Model(const VulkanState &state, vk::RenderPass render_pass, std::string_view filename) noexcept
{
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  std::filesystem::path bin_dir = std::filesystem::current_path().append("bin");
  std::filesystem::path model_path = bin_dir.append("models").append(filename);
  std::filesystem::path shader_path = bin_dir.append("shaders").append("default");

  bool ret;
  if (model_path.extension() == ".gltf") {
    ret = loader.LoadASCIIFromFile(&model, &err, &warn, model_path.string());
  }
  else {
    ret = loader.LoadBinaryFromFile(&model, &err, &warn, model_path.string());
  }
  if (!ret) {
    exit(-1);
  }

  std::array attributes {
    "POSITION",
    "NORMAL",
  };

  for (auto &mesh : model.meshes) {
    for (auto &primitive : mesh.primitives) {
      struct Vertex {
        float posx, posy, posz;
        float normx, normy, normz;
      };

      std::vector<Vertex> vertexes;
      VertexBuffer vbuf;
      IndexBuffer ibuf;

      for (auto attribute : attributes) {
        if (primitive.attributes.find(attribute) != primitive.attributes.end()) {
          const auto &accessor = model.accessors[primitive.attributes[attribute]];
          if (vertexes.size() == 0) {
            vertexes.resize(accessor.count);
          }
          const auto &view = model.bufferViews[accessor.bufferView];
          const size_t attribute_size = 3;
          const float *attribute_buffer = (float *)&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
          const size_t attribute_count = accessor.count;
          for (int i = 0; i < vertexes.size(); i++) {
            if (std::strcmp(attribute, "POSITION") == 0) {
              vertexes[i].posx = attribute_buffer[i * 3 + 0];
              vertexes[i].posy = attribute_buffer[i * 3 + 1];
              vertexes[i].posz = attribute_buffer[i * 3 + 2];
            }
            else if (std::strcmp(attribute, "NORMAL") == 0) {
              vertexes[i].normx = attribute_buffer[i * 3 + 0];
              vertexes[i].normy = attribute_buffer[i * 3 + 1];
              vertexes[i].normz = attribute_buffer[i * 3 + 2];
            }
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

      _meshes.emplace_back(std::move(vbuf), std::move(ibuf), index_count);
      _materials.emplace_back(handle_material(state, render_pass));
    }
  }
}

void mr::Model::draw(CommandUnit &unit) const noexcept {
  for (const auto &[material, mesh] : std::views::zip(_materials, _meshes)) {
    material.bind(unit);
    unit->bindVertexBuffers(0, mesh.vbuf().buffer(), {0});
    unit->bindIndexBuffer(mesh.ibuf().buffer(), 0, mesh.ibuf().index_type());
    unit->drawIndexed(mesh.num_of_indices(), 1, 0, 0, 0);
  }
}

