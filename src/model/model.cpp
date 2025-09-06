#include "model/model.hpp"
#include "renderer/renderer.hpp"

// TODO(dk6): workaround to pass camera data to material ubo until Scene class will be added
extern mr::UniformBuffer *s_cam_ubo_ptr;

mr::graphics::Model::Model(const VulkanState &state, const RenderContext &render_context, std::string_view filename) noexcept
{
  MR_INFO("Loading model {}", filename);

  std::filesystem::path model_path = path::models_dir / filename;

  auto model = mr::import(model_path);
  if (!model) {
    MR_ERROR("Loading model {} failed", model_path.string());
    return;
  }
  auto& model_value = model.value();

  MR_INFO("Loading model {} finished\n", filename);
}

void mr::graphics::Model::draw(CommandUnit &unit) const noexcept
{
  /*
  for (const auto &[material, mesh] : std::views::zip(_materials, _meshes)) {
    material->bind(unit);
    mesh.bind(unit);
    mesh.draw(unit);
  }
  */
}
