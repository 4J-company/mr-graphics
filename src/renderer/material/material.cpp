#include "material/material.hpp"

/**
 * @brief Constructs a Material and initializes its rendering resources.
 *
 * This constructor sets up a material for rendering by initializing the uniform buffers,
 * shader handle, and descriptor allocator. It creates descriptor attachments for the camera
 * uniform buffer, the material's own uniform buffer, and any additional textures provided.
 * A descriptor set is then allocated for the vertex shader stage based on these resource views,
 * and a graphics pipeline is configured for opaque geometry rendering.
 *
 * @param state A reference to the Vulkan context.
 * @param render_pass The Vulkan render pass used for pipeline creation.
 * @param shader The shader handle used for rendering the material.
 * @param ubo_data A span containing uniform buffer object data for the material.
 * @param textures A span of optional texture handles associated with the material.
 * @param cam_ubo A reference to the camera uniform buffer.
 */
mr::Material::Material(const VulkanState &state, const vk::RenderPass render_pass,
                       mr::ShaderHandle shader,
                       std::span<float> ubo_data,
                       std::span<std::optional<mr::TextureHandle>> textures,
                       mr::UniformBuffer &cam_ubo) noexcept
    : _ubo(state, ubo_data)
    , _shader(std::move(shader))
    , _descriptor_allocator(state)
{
  std::vector<Shader::ResourceView> attachments;
  attachments.reserve(textures.size());

  attachments.emplace_back(0, 0, &cam_ubo);
  attachments.emplace_back(0, 1, &_ubo);
  for (size_t i = 0; i < textures.size(); i++) {
    if (!textures[i].has_value()) {
      continue;
    }
    attachments.emplace_back(0, static_cast<uint32_t>(i + 2), textures[i].value().get());
  }

  _descriptor_set =
    _descriptor_allocator
      .allocate_set(Shader::Stage::Vertex, std::span {attachments})
      .value();

  std::array layouts = {_descriptor_set.layout()};

  _pipeline =
    mr::GraphicsPipeline(state,
                         render_pass,
                         mr::GraphicsPipeline::Subpass::OpaqueGeometry,
                         _shader,
                         std::span {_descrs},
                         std::span {layouts});
}

/**
 * @brief Binds the material's graphics pipeline and descriptor sets to a command unit.
 *
 * This method activates the material's graphics pipeline and associated descriptor set, ensuring
 * that the appropriate rendering state and resources are used for subsequent draw calls.
 *
 * @param unit The command unit to which the pipeline and descriptor sets are bound.
 */
void mr::Material::bind(CommandUnit &unit) const noexcept
{
  unit->bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline.pipeline());
  unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                           {_pipeline.layout()},
                           0,
                           {_descriptor_set.set()},
                           {});
}
