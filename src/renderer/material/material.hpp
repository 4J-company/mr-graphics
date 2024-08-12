#ifndef __material_cpp_
#define __material_cpp_

#include "pch.hpp"
#include "resources/resources.hpp"

namespace mr {
  class Material {
  protected:
    mr::UniformBuffer _ubo;
    mr::Shader _shader;

    mr::DescriptorAllocator _descriptor_allocator; 
    mr::DescriptorSet _descriptor_set; 
    mr::GraphicsPipeline _pipeline;

    std::array<vk::VertexInputAttributeDescription, 3> _descrs {
      vk::VertexInputAttributeDescription {
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = 0
      },
      vk::VertexInputAttributeDescription {
        .location = 1,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = 3 * sizeof(float)
      },
      vk::VertexInputAttributeDescription {
        .location = 2,
        .binding = 0,
        .format = vk::Format::eR32G32Sfloat,
        .offset = 6 * sizeof(float)
      },
    };

  public:
    Material(const VulkanState state, const vk::RenderPass render_pass, Shader shader,
             std::span<float> ubo_data, std::span<std::optional<mr::Texture>> textures) noexcept;

    void bind(CommandUnit &unit) const noexcept;
  };

  class MaterialBuilder {
    mr::VulkanState _state;
    vk::RenderPass _render_pass;

    std::vector<byte> _specialization_data;
    std::unordered_map<std::string, std::string> _defines;
    std::vector<float> _ubo_data;
    std::vector<std::optional<mr::Texture>> _textures;

    std::string_view _shader_filename;

    void _add_shader_define(const std::string &name, const std::string &value) noexcept {
      _defines[name] = value;
    }

  public:
    MaterialBuilder(
      const mr::VulkanState &state,
      const vk::RenderPass &render_pass,
      std::string_view filename)
        : _state(state)
        , _render_pass(render_pass)
        , _shader_filename(filename)
    {
    }

    MaterialBuilder(MaterialBuilder &&) noexcept = default;
    MaterialBuilder & operator=(MaterialBuilder &&) noexcept = default;

    MaterialBuilder &add_texture(const std::string &name,
                                 std::optional<mr::Texture> tex,
                                 mr::Vec4f fallback = {0, 0, 0, 0})
    {
      _add_shader_define(name + "_PRESENT", std::to_string(tex.has_value()));
      add_value(fallback);
      _textures.emplace_back(std::move(tex));
      return *this;
    }

    MaterialBuilder & add_value(const auto &value)
    {
      _ubo_data.append_range(std::span {(float *)&value, sizeof(value)});
      return *this;
    }

    MaterialBuilder & add_value(float value)
    {
      _ubo_data.push_back(value);
      return *this;
    }

    Material build() noexcept
    {
      return Material {
        _state,
        _render_pass,
        mr::Shader(_state,
                   _shader_filename,
                   _defines),
        std::span {_ubo_data},
        std::span {_textures}
      };
    }
  };
} // namespace mr

#endif __material_cpp_
