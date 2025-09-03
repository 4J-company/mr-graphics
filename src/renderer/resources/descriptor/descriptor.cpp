#include "resources/descriptor/descriptor.hpp"
#include "resources/shaders/shader.hpp"

// ============================================================================
// Static functions
// ============================================================================

static vk::DescriptorType get_descriptor_type(const mr::Shader::Resource &attachment) noexcept
{
  using enum vk::DescriptorType;
  static std::array types {
    eUniformBuffer,
    eStorageBuffer,
    eCombinedImageSampler,
    eInputAttachment,
  };

  ASSERT(attachment.index() < types.size());
  return types[attachment.index()];
}

static std::vector<vk::DescriptorSetLayoutBinding>
get_bindings(vk::ShaderStageFlags stage,
             std::span<const mr::Shader::ResourceView> attachment_set) noexcept
{
  return attachment_set | std::views::transform([stage](auto &&set) {
    return vk::DescriptorSetLayoutBinding {
      .binding = set.binding,
      .descriptorType = get_descriptor_type(set),
      .descriptorCount = 1,
      .stageFlags = stage,
      .pImmutableSamplers = nullptr, // TODO: replace this with corresponding sampler as it's never changed
    };
  }) | std::ranges::to<std::vector>();
}

// ============================================================================
// Descriptor set layout functions
// ============================================================================

mr::DescriptorSetLayout::DescriptorSetLayout(const VulkanState &state,
                                             vk::ShaderStageFlags stage,
                                             std::span<const Shader::ResourceView> attachments) noexcept
{
  auto set_bindings = get_bindings(stage, attachments);

  // Create descriptor set layout
  vk::DescriptorSetLayoutCreateInfo set_layout_create_info {
    .bindingCount = static_cast<uint32_t>(set_bindings.size()),
    .pBindings = set_bindings.data(),
  };

  auto [res, set_layout] =
    state.device().createDescriptorSetLayoutUnique(set_layout_create_info);
  ASSERT(res == vk::Result::eSuccess);
  _set_layout = std::move(set_layout);
}

// ============================================================================
// Descriptor set functions
// ============================================================================

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
  auto write_uniform_buffer = [&](UniformBuffer *buffer, vk::DescriptorBufferInfo &info) {
    write_buffer(buffer, info);
  };
  auto write_storage_buffer = [&](StorageBuffer *buffer, vk::DescriptorBufferInfo &info) {
    write_buffer(buffer, info);
  };
  auto write_texture = [&](Texture *texture, vk::DescriptorImageInfo &info) {
    info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    info.imageView = texture->image().image_view();
    info.sampler = texture->sampler().sampler();
  };
  auto write_geometry_buffer = [&](Image *image, vk::DescriptorImageInfo &info) {
    info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    info.imageView = image->image_view();
    info.sampler = nullptr;
  };
  auto write_default = [](auto, auto) { std::unreachable(); };

  for (const auto &[attachment_view, write_info] : std::views::zip(attachments, write_infos)) {
    const Shader::Resource &attachment = attachment_view;
    if (std::holds_alternative<Texture *>(attachment) || std::holds_alternative<Image *>(attachment)) {
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

  static constexpr size_t max_descriptor_writes = 64;
  ASSERT(attachments.size() < max_descriptor_writes);
  beman::inplace_vector<vk::WriteDescriptorSet, max_descriptor_writes> descriptor_writes;

  for (const auto &[attach, write_info] : std::views::zip(attachments, write_infos)) {
    descriptor_writes.emplace_back(vk::WriteDescriptorSet {
      .dstSet = _set,
      .dstBinding = attach.binding,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = get_descriptor_type(attach),
      .pImageInfo = std::get_if<vk::DescriptorImageInfo>(&write_info),
      .pBufferInfo = std::get_if<vk::DescriptorBufferInfo>(&write_info),
    });
  }

  state.device().updateDescriptorSets(descriptor_writes, {});
}

// ============================================================================
// Descriptor allocator functions
// ============================================================================

mr::DescriptorAllocator::DescriptorAllocator(const VulkanState &state)
    : _state(&state)
{
  static constexpr std::array default_sizes {
    vk::DescriptorPoolSize {vk::DescriptorType::eUniformBuffer, 10},
    vk::DescriptorPoolSize {vk::DescriptorType::eStorageBuffer, 10},
    vk::DescriptorPoolSize {vk::DescriptorType::eSampledImage, 5},
    vk::DescriptorPoolSize {vk::DescriptorType::eInputAttachment, 10},
    vk::DescriptorPoolSize {vk::DescriptorType::eCombinedImageSampler, 10},
    // vk::DescriptorPoolSize {vk::DescriptorType::eUniformBufferDynamic, 5},
    // vk::DescriptorPoolSize {vk::DescriptorType::eStorageBufferDynamic, 5},
  };

  if (auto pool = allocate_pool(default_sizes); pool.has_value()) {
    _pools.emplace_back(std::move(pool.value()));
  }
}

std::optional<vk::UniqueDescriptorPool> mr::DescriptorAllocator::allocate_pool(
  std::span<const vk::DescriptorPoolSize> sizes) noexcept
{
  // Create descriptor pool
  vk::DescriptorPoolCreateInfo pool_create_info {
    .maxSets = 1'000,
    .poolSizeCount = (uint32_t)sizes.size(),
    .pPoolSizes = sizes.data(),
  };

  auto [res, pool] = _state->device().createDescriptorPoolUnique(pool_create_info);
  if (res != vk::Result::eSuccess) [[unlikely]] {
    return std::nullopt;
  }
  return std::move(pool);
}

// TODO: rewrite allocate_set in a 1-dimensional manner
std::optional<mr::DescriptorSet> mr::DescriptorAllocator::allocate_set(DescriptorSetLayoutHandle set_layout) const noexcept
{
  return allocate_sets({&set_layout, 1}).transform(
    [](std::vector<mr::DescriptorSet> &&v) -> mr::DescriptorSet {
      return std::move(v[0]);
    });
}

// TODO: handle resizing pool vector
std::optional<std::vector<mr::DescriptorSet>> mr::DescriptorAllocator::allocate_sets(
  std::span<const DescriptorSetLayoutHandle> set_layouts) const noexcept
{
  static constexpr size_t max_descriptor_set_number = 64;
  ASSERT(set_layouts.size() < max_descriptor_set_number);
  beman::inplace_vector<vk::DescriptorSetLayout, max_descriptor_set_number> layouts;

  std::vector<DescriptorSet> sets;
  sets.reserve(set_layouts.size());

  for (const auto &layout : set_layouts) {
    layouts.emplace_back(layout->layout());
    // TODO(dk6): check this 0
    sets.emplace_back(vk::DescriptorSet(), layout, 0);
  }

  vk::DescriptorSetAllocateInfo descriptor_alloc_info {
    .descriptorPool = _pools.back().get(),
    .descriptorSetCount = static_cast<uint32_t>(sets.size()),
    .pSetLayouts = layouts.data(),
  };
  auto [res, val] =
    _state->device().allocateDescriptorSets(descriptor_alloc_info);
  if (res != vk::Result::eSuccess) [[unlikely]] {
    return std::nullopt;
  }

  // fill output variable with created vk::DescriptorSet's
  for (int i = 0; i < sets.size(); i++) {
    sets[i]._set = val[i];
  }

  return std::move(sets);
}

void mr::DescriptorAllocator::reset() noexcept
{
  for (auto &pool : _pools) {
    _state->device().resetDescriptorPool(pool.get());
  }
}
