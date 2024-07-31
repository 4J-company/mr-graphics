#ifndef __material_cpp_
#define __material_cpp_

#include "pch.hpp"
#include "resources/resources.hpp"

namespace mr {
  class Material {
  private:
    Matr4f _transform;
    Vec3f _ambient_color;
    Vec3f _diffuse_color;
    Vec3f _specular_color;
    float _shininess;

    std::span<float> _attachments = {(float *)&_transform, std::size_t(&((Material*)nullptr)->_attachments)};
    mr::Shader _shader;
    mr::UniformBuffer _ubo;

    DescriptorAllocator _descriptor_allocator;
    DescriptorSet _descriptor_set;

    mr::GraphicsPipeline _pipeline;

  public:
    Material() noexcept = default;

    Material(const VulkanState &state, vk::RenderPass render_pass,
             Shader shader, Vec3f ambient, Vec3f diffuse, Vec3f specular,
             float shininess, Matr4f transform)
        : _transform(transform)
        , _ambient_color(ambient)
        , _diffuse_color(diffuse)
        , _specular_color(specular)
        , _shininess(shininess)
        , _ubo(state, _attachments)
        , _descriptor_allocator(state)
        , _shader(std::move(shader))
    {
      const std::array descrs {
        vk::VertexInputAttributeDescription{
          .location = 0,
          .binding = 0,
          .format = vk::Format::eR32G32B32Sfloat,
          .offset = 0
        },
        vk::VertexInputAttributeDescription{
          .location = 1,
          .binding = 0,
          .format = vk::Format::eR32G32B32Sfloat,
          .offset = 3 * sizeof(float)
        },
 /*
        vk::VertexInputAttributeDescription{
          .location = 2,
          .binding = 0,
          .format = vk::Format::eR32G32B32Sfloat,
          .offset = 6 * sizeof(float)},
        vk::VertexInputAttributeDescription{
          .location = 3,
          .binding = 0,
          .format = vk::Format::eR32G32B32Sfloat,
          .offset = 9 * sizeof(float)
        },
        vk::VertexInputAttributeDescription{
          .location = 4,
          .binding = 0,
          .format = vk::Format::eR32G32B32Sfloat,
          .offset = 12 * sizeof(float)
        },
        vk::VertexInputAttributeDescription{
          .location = 5,
          .binding = 0,
          .format = vk::Format::eR32G32B32Sfloat,
          .offset = 15 * sizeof(float)
        },
*/
      };

      const std::array vertex_attachments {
        Shader::ResourceView {0, 0, &_ubo}, // set, binding, res
      };

      _descriptor_set =
        _descriptor_allocator
          .allocate_set(Shader::Stage::Vertex, std::span {vertex_attachments})
          .value();

      std::array layouts = {_descriptor_set.layout()};

      _pipeline =
        mr::GraphicsPipeline(state,
                             render_pass,
                             mr::GraphicsPipeline::Subpass::OpaqueGeometry,
                             &_shader,
                             std::span {descrs},
                             std::span {layouts});
    }

    void bind(CommandUnit &unit) const noexcept
    {
      unit->bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline.pipeline());
      unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               _pipeline.layout(),
                               0,
                               {_descriptor_set.set()},
                               {});
    }
  };
} // namespace mr

#endif __material_cpp_
