#include "resources/descriptor/descriptor.hpp"
#include "resources/shaders/shader.hpp"

static constexpr
vk::DescriptorType get_descriptor_type(const mr::Shader::Resource &attachment) noexcept
{
  using enum vk::DescriptorType;
  static constexpr std::array types {
    eUniformBuffer,
    eStorageBuffer,
    eCombinedImageSampler,
    eInputAttachment,
  };

  return types[attachment.index()];
}

static constexpr std::vector<vk::DescriptorSetLayoutBinding>
get_bindings(mr::Shader::Stage stage, std::span<mr::Shader::ResourceView> attachment_set)
{
  std::vector<vk::DescriptorSetLayoutBinding> set_bindings(
    attachment_set.size());
  for (auto &resource_view : attachment_set) {
    vk::DescriptorSetLayoutBinding binding {
      .binding = resource_view.binding,
      .descriptorType = get_descriptor_type(resource_view.res),
      .descriptorCount = 1, // TODO: replace with reasonable value
      .stageFlags = get_stage_flags(stage),
      .pImmutableSamplers = nullptr, // TODO: investigate immutable samplers
    };

    set_bindings.emplace_back(std::move(binding));
  }

  return std::move(set_bindings);
}

void mr::DescriptorSet::update(
  const VulkanState &state, std::span<Shader::ResourceView> attachments) noexcept
{
  using WriteInfo =
    std::variant<vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;
  std::vector<WriteInfo> write_infos(attachments.size());

  auto write_host_buffer = [&](HostBuffer *buffer,
                               vk::DescriptorBufferInfo &info) {
    info.buffer = buffer->buffer();
    info.range = buffer->byte_size();
    info.offset = 0;
  };
  auto write_device_buffer = [&](DeviceBuffer *buffer,
                                 vk::DescriptorBufferInfo &info) {
    info.buffer = buffer->buffer();
    info.range = buffer->byte_size();
    info.offset = 0;
  };
  auto write_uniform_buffer = [&](UniformBuffer *buffer,
                                  vk::DescriptorBufferInfo &info) {
    write_host_buffer(buffer, info);
  };
  auto write_storage_buffer = [&](StorageBuffer *buffer,
                                  vk::DescriptorBufferInfo &info) {
    write_device_buffer(buffer, info);
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
    descriptor_writes[i] = vk::WriteDescriptorSet {
      .dstSet = _set,
      .dstBinding = i,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = get_descriptor_type(attachments[i]),
      .pImageInfo = std::get_if<vk::DescriptorImageInfo>(&write_infos[i]),
      .pBufferInfo = std::get_if<vk::DescriptorBufferInfo>(&write_infos[i]),
    };
  }

  state.device().updateDescriptorSets(descriptor_writes, {});
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

  auto [res, pool] = _state.device().createDescriptorPoolUnique(pool_create_info);
  if (res != vk::Result::eSuccess) [[unlikely]] {
    return std::nullopt;
  }
  return std::move(pool);
}

// TODO: rewrite allocate_set in a 1-dimensional manner
std::optional<mr::DescriptorSet> mr::DescriptorAllocator::allocate_set(
  Shader::Stage stage, std::span<Shader::ResourceView> attachments) noexcept
{
  std::pair p = {stage, attachments};
  return allocate_sets({&p, 1})
    .transform([](std::vector<mr::DescriptorSet> v) -> mr::DescriptorSet {
      return std::move(v[0]);
    });
}

// TODO: handle resizing pool vector
std::optional<std::vector<mr::DescriptorSet>>
mr::DescriptorAllocator::allocate_sets(
  std::span<std::pair<Shader::Stage, std::span<Shader::ResourceView>>> attachment_sets) noexcept
{
  std::vector<vk::DescriptorSetLayout> layouts(attachment_sets.size());
  std::vector<mr::DescriptorSet> sets(attachment_sets.size());

  for (auto &attachment_set : attachment_sets) {
    auto set_bindings = get_bindings(attachment_set.first, attachment_set.second);

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
