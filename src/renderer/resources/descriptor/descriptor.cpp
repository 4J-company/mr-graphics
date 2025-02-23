#include "resources/descriptor/descriptor.hpp"
#include "resources/shaders/shader.hpp"

static vk::DescriptorType
get_descriptor_type(const mr::Shader::Resource &attachment) noexcept
{
  using enum vk::DescriptorType;
  static std::array types {
    eUniformBuffer,
    eStorageBuffer,
    eCombinedImageSampler,
    eInputAttachment,
  };

  assert(attachment.index() < types.size());
  return types[attachment.index()];
}

/**
 * @brief Creates descriptor set layout bindings for a set of shader resource views.
 *
 * Iterates through the provided span of shader resource views and constructs a corresponding
 * vector of Vulkan descriptor set layout bindings. Each binding is configured with:
 * - The binding index from the resource view.
 * - A descriptor type determined by the resource view.
 * - A fixed descriptor count of 1.
 * - Shader stage flags derived from the provided shader stage.
 * - An immutable sampler pointer set to nullptr (with a note to replace it with the appropriate sampler).
 *
 * @param stage The shader stage for which the descriptor layout bindings are created.
 * @param attachment_set A span of shader resource views representing the attachments.
 * @return std::vector<vk::DescriptorSetLayoutBinding> The vector of configured descriptor set layout bindings.
 */
static std::vector<vk::DescriptorSetLayoutBinding>
get_bindings(mr::Shader::Stage stage,
             std::span<const mr::Shader::ResourceView> attachment_set) noexcept
{
  std::vector<vk::DescriptorSetLayoutBinding> set_bindings(
    attachment_set.size());
  for (int i = 0; i < attachment_set.size(); i++) {
    set_bindings[i] = vk::DescriptorSetLayoutBinding {
      .binding = attachment_set[i].binding,
      .descriptorType = get_descriptor_type(attachment_set[i]),
      .descriptorCount = 1,
      .stageFlags = get_stage_flags(stage),
      .pImmutableSamplers = nullptr, // TODO: replace this with corresponding sampler as it's never changed
    };
  }

  return set_bindings;
}

/**
 * @brief Updates the descriptor set with new shader resource attachments.
 *
 * Processes a span of shader resource views by preparing and applying corresponding write
 * descriptor structures. Depending on the resource type (buffer or image), the function populates
 * the appropriate descriptor information and updates the underlying Vulkan descriptor set.
 *
 * @param attachments Span of shader resource views specifying the resources to bind.
 */
void mr::DescriptorSet::update(
  const VulkanState &state,
  std::span<const Shader::ResourceView> attachments) noexcept
{
  using WriteInfo =
    std::variant<vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;
  std::vector<WriteInfo> write_infos(attachments.size());

  auto write_buffer = [&](Buffer *buffer, vk::DescriptorBufferInfo &info) {
    info.buffer = buffer->buffer();
    info.range = buffer->byte_size();
    info.offset = 0;
  };
  auto write_uniform_buffer = [&](UniformBuffer *buffer,
                                  vk::DescriptorBufferInfo &info) {
    write_buffer(buffer, info);
  };
  auto write_storage_buffer = [&](StorageBuffer *buffer,
                                  vk::DescriptorBufferInfo &info) {
    write_buffer(buffer, info);
  };
  auto write_texture = [&](Texture *texture, vk::DescriptorImageInfo &info) {
    info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    info.imageView = texture->image().image_view();
    info.sampler = texture->sampler().sampler();
  };
  auto write_geometry_buffer = [&](Image *image,
                                   vk::DescriptorImageInfo &info) {
    info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    info.imageView = image->image_view();
    info.sampler = nullptr;
  };
  auto write_default = [](auto, auto) { std::unreachable(); };

  for (const auto &[attachment_view, write_info] :
       std::views::zip(attachments, write_infos)) {
    const Shader::Resource &attachment = attachment_view;
    if (std::holds_alternative<Texture *>(attachment) ||
        std::holds_alternative<Image *>(attachment)) {
      write_info.emplace<vk::DescriptorImageInfo>();
    }

    std::visit(Overloads {write_uniform_buffer,
                          write_storage_buffer,
                          write_texture,
                          write_geometry_buffer,
                          write_default},
               attachment,
               write_info);
  }

  std::vector<vk::WriteDescriptorSet> descriptor_writes(attachments.size());
  for (uint i = 0; i < attachments.size(); i++) {
    descriptor_writes[i] = {
      .dstSet = _set,
      .dstBinding = attachments[i].binding,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = get_descriptor_type(attachments[i]),
      .pImageInfo = std::get_if<vk::DescriptorImageInfo>(&write_infos[i]),
      .pBufferInfo = std::get_if<vk::DescriptorBufferInfo>(&write_infos[i]),
    };
  }

  state.device().updateDescriptorSets(descriptor_writes, {});
}

mr::DescriptorAllocator::DescriptorAllocator(const VulkanState &state)
    : _state(state)
{
  static std::vector<vk::DescriptorPoolSize> default_sizes = {
    {vk::DescriptorType::eUniformBuffer, 10},
    { vk::DescriptorType::eSampledImage,  5},
  };

  if (auto pool = allocate_pool(default_sizes); pool.has_value()) {
    _pools.emplace_back(std::move(pool.value()));
  }
}

std::optional<vk::UniqueDescriptorPool> mr::DescriptorAllocator::allocate_pool(
  std::span<vk::DescriptorPoolSize> sizes) noexcept
{
  // Create descriptor pool
  vk::DescriptorPoolCreateInfo pool_create_info {
    .maxSets = 1'000,
    .poolSizeCount = (uint32_t)sizes.size(),
    .pPoolSizes = sizes.data(),
  };

  auto [res, pool] =
    _state.device().createDescriptorPoolUnique(pool_create_info);
  if (res != vk::Result::eSuccess) [[unlikely]] {
    return std::nullopt;
  }
  return std::move(pool);
}

// TODO: rewrite allocate_set in a 1-dimensional manner
std::optional<mr::DescriptorSet> mr::DescriptorAllocator::allocate_set(
  Shader::Stage stage,
  std::span<const Shader::ResourceView> attachments) noexcept
{
  std::pair p = {stage, attachments};
  return allocate_sets({&p, 1}).transform(
    [](std::vector<mr::DescriptorSet> &&v) -> mr::DescriptorSet {
      return std::move(v[0]);
    });
}

// TODO: handle resizing pool vector
std::optional<std::vector<mr::DescriptorSet>>
mr::DescriptorAllocator::allocate_sets(
  std::span<
    const std::pair<Shader::Stage, std::span<const Shader::ResourceView>>>
    attachment_sets) noexcept
{
  std::vector<vk::DescriptorSetLayout> layouts;
  std::vector<mr::DescriptorSet> sets;

  layouts.reserve(attachment_sets.size());
  sets.reserve(attachment_sets.size());

  for (auto &attachment_set : attachment_sets) {
    auto set_bindings =
      get_bindings(attachment_set.first, attachment_set.second);

    // Create descriptor set layout
    vk::DescriptorSetLayoutCreateInfo set_layout_create_info {
      .bindingCount = (uint32_t)set_bindings.size(),
      .pBindings = set_bindings.data(),
    };

    auto [res, set_layout] =
      _state.device().createDescriptorSetLayoutUnique(set_layout_create_info);
    assert(res == vk::Result::eSuccess);

    sets.emplace_back(vk::DescriptorSet(), std::move(set_layout), 0);
    layouts.emplace_back(sets.back().layout());
  }

  vk::DescriptorSetAllocateInfo descriptor_alloc_info {
    .descriptorPool = _pools.back().get(),
    .descriptorSetCount = (uint32_t)attachment_sets.size(),
    .pSetLayouts = layouts.data(),
  };
  auto [res, val] =
    _state.device().allocateDescriptorSets(descriptor_alloc_info);
  if (res != vk::Result::eSuccess) [[unlikely]] {
    return std::nullopt;
  }

  // fill output variable with created vk::DescriptorSet's
  for (int i = 0; i < sets.size(); i++) {
    sets[i]._set = val[i];
    sets[i].update(_state, attachment_sets[i].second);
  }

  return std::move(sets);
}

void mr::DescriptorAllocator::reset() noexcept
{
  for (int i = 0; i < _pools.size(); i++) {
    _state.device().resetDescriptorPool(_pools[i].get());
  }
}
