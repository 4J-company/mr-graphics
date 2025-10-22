#include "material/material.hpp"
#include "window/render_context.hpp"

// ----------------------------------------------------------------------------
// Material
// ----------------------------------------------------------------------------

static size_t get_aligned_16_byte(size_t size) {
  return size % 4 == 0 ? size : ((size / 4 + 1) * 4);
}

mr::graphics::Material::Material(RenderContext &render_context,
                                 mr::graphics::ShaderHandle shader,
                                 std::span<std::byte> ubo_data,
                                 std::span<std::optional<mr::TextureHandle>> textures,
                                 std::span<mr::StorageBuffer *> storage_buffers,
                                 std::span<mr::ConditionalBuffer *> conditional_buffers) noexcept
    : _ubo(render_context.vulkan_state(),
           ubo_data.size() + sizeof(uint32_t) * get_aligned_16_byte(textures.size()))
    , _shader(shader)
    , _context(&render_context)
{
  ASSERT(_shader.get() != nullptr, "Invalid shader passed to the material", _shader->name());

  std::ranges::copy(textures, _textures.begin());

  std::array layouts { render_context.bindless_set_layout() };
  _pipeline = mr::GraphicsPipeline(render_context,
                                   mr::GraphicsPipeline::Subpass::OpaqueGeometry,
                                   _shader,
                                   std::span {mr::importer::Mesh::vertex_input_attribute_descriptions},
                                   std::span {layouts});

  // TODO: also register storage and conditional buffers
  constexpr size_t max_textures_size = enum_cast(MaterialParameter::EnumSize);
  InplaceVector<Shader::Resource, max_textures_size + 1> resources;
  for (auto &tex : textures) {
    if (tex.has_value()) {
      resources.push_back(tex.value().get());
    }
  }
  resources.push_back(&_ubo);

  auto resources_ids = render_context.bindless_set().register_resources(resources);

  std::ranges::fill(_textures_ids, -1);
  int index = 0;
  for (auto &&[texture, id] : std::views::zip(textures, _textures_ids)) {
    if (texture.has_value()) {
      id = resources_ids[index++];
    }
  }
  _uniform_buffer_id = resources_ids.back();

  std::vector<std::byte> ubo_data_with_texs(_ubo.byte_size());
  std::ranges::copy(ubo_data, ubo_data_with_texs.begin());
  std::memcpy(&ubo_data_with_texs[ubo_data.size()],
              _textures_ids.data(),
              textures.size() * sizeof(uint32_t));
  _ubo.write(ubo_data_with_texs);
}

mr::graphics::Material::~Material()
{
  for (auto &tex : _textures) {
    if (tex.has_value()) {
      _context->bindless_set().unregister_resource(tex.value().get());
    }
  }
  _context->bindless_set().unregister_resource(&_ubo);
}

void mr::Material::bind(CommandUnit &unit) const noexcept
{
  unit->bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline.pipeline());
  unit->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                           {_pipeline.layout()},
                           0,
                           {_context->bindless_set().set()}, // here must be descriptor set
                           {});
}

// ----------------------------------------------------------------------------
// Material Builder
// ----------------------------------------------------------------------------

mr::MaterialBuilder::MaterialBuilder(const mr::VulkanState &state,
                                     mr::RenderContext &context,
                                     std::string_view filename)
  : _state(&state)
  , _context(&context)
  , _shader_filename(filename)
{
}

mr::MaterialBuilder & mr::MaterialBuilder::add_texture(MaterialParameter param,
                                                       const mr::importer::TextureData &tex_data,
                                                       math::Color factor)
{
  ASSERT(tex_data.image.pixels.get() != nullptr, "Image should be valid");
  mr::TextureHandle tex = ResourceManager<Texture>::get().create(mr::unnamed,
    _context->vulkan_state(),
    tex_data.image
  );

  ASSERT(enum_cast(param) < enum_cast(MaterialParameter::EnumSize));
  _textures[enum_cast(param)] = std::move(tex);
  // add_value(factor);
  return *this;
}

mr::MaterialHandle mr::MaterialBuilder::build() noexcept
{
  auto &mtlmanager = ResourceManager<Material>::get();
  auto &shdmanager = ResourceManager<Shader>::get();

  auto definestr = generate_shader_defines_str();
  auto shdname = std::string(_shader_filename) + ":" + definestr;
  auto shdfindres = shdmanager.find(shdname);
  auto shdhandle = shdfindres ? shdfindres : shdmanager.create(shdname, *_state, _shader_filename, generate_shader_defines());

  ASSERT(_state != nullptr);
  ASSERT(_context != nullptr);

  return mtlmanager.create(unnamed,
    *_context,
    shdhandle,
    std::span {_ubo_data},
    std::span {_textures},
    std::span {_storage_buffers.data(), _storage_buffers.size()},
    std::span {_conditional_buffers.data(), _conditional_buffers.size()}
  );
}

boost::unordered_map<std::string, std::string> mr::MaterialBuilder::generate_shader_defines() const noexcept
{
  boost::unordered_map<std::string, std::string> defines;
  for (size_t i = 0; i < enum_cast(MaterialParameter::EnumSize); i++) {
    if (_textures[i].has_value()) {
      defines[get_material_parameter_define(enum_cast<MaterialParameter>(i))] = std::to_string(i + 2);
    }
  }
  defines["TEXTURES_BINDING"] = std::to_string(RenderContext::textures_binding);
  defines["UNIFORM_BUFFERS_BINDING"] = std::to_string(RenderContext::uniform_buffer_binding);
  defines["STORAGE_BUFFERS_BINDING"] = std::to_string(RenderContext::storage_buffer_binding);
  return defines;
}
