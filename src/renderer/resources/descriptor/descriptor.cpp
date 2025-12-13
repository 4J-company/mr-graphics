#include "resources/images/image.hpp"
#include "resources/texture/sampler/sampler.hpp"

#include "resources/descriptor/descriptor.hpp"
#include "resources/shaders/shader.hpp"

#include "window/render_context.hpp"

// ============================================================================
// Static functions
// ============================================================================

static vk::DescriptorType get_descriptor_type(const mr::graphics::Shader::Resource &attachment) noexcept
{
  using enum vk::DescriptorType;
  static std::array types {
    eUniformBuffer,        // for UniformBuffer
    eStorageBuffer,        // for StorageBuffer
    eCombinedImageSampler, // for Sampler
    eInputAttachment,      // for ColorAttachmentImage?
    eStorageImage,         // for StorageImage
    eCombinedImageSampler, // for SamplerStorageImage
    eStorageImage,         // for PyramidImage
    eStorageImage,         // for DepthImage
    eStorageBuffer,        // for ConditionalBuffer
  };

  ASSERT(attachment.index() < types.size());
  return types[attachment.index()];
}

// ============================================================================
// Descriptor set layout functions
// ============================================================================

void mr::DescriptorSetLayout::fill_binding(CreateBindingsList &set_bindings,
                                           const vk::ShaderStageFlags stage,
                                           std::span<const BindingDescription> bindings,
                                           uint32_t descriptor_count) noexcept
{
  ASSERT(bindings.size() < desciptor_set_max_bindings,
    "Max binding value is desciptor_set_max_bindings and all must be unique");
  std::array<bool, desciptor_set_max_bindings> used_bindings {};

  for (auto &&[binding, type] : bindings) {
    ASSERT(binding < desciptor_set_max_bindings, "binding value {} is too big,"
      "to fix this error you can increase value of 'desciptor_set_max_bindings' in descriptor.hpp",
      binding);
    ASSERT(used_bindings[binding] == false, "binding {} appears at least twice", binding);
    used_bindings[binding] = true;

    set_bindings.emplace_back(vk::DescriptorSetLayoutBinding {
      .binding = binding,
      .descriptorType = type,
      .descriptorCount = descriptor_count,
      .stageFlags = stage,
      .pImmutableSamplers = nullptr, // TODO: replace this with corresponding sampler as it's never changed
    });
  };

  _bindings.resize(desciptor_set_max_bindings);
  for (auto [binding, type] : bindings) {
    _bindings[binding] = type;
  }
}

mr::DescriptorSetLayout::DescriptorSetLayout(const VulkanState &state,
                                             vk::ShaderStageFlags stage,
                                             std::span<const BindingDescription> bindings) noexcept
{
  InplaceVector<vk::DescriptorSetLayoutBinding, desciptor_set_max_bindings> set_bindings;
  fill_binding(set_bindings, stage, bindings, 1);

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
// Bindless descriptor set layout functions
// ============================================================================


mr::BindlessDescriptorSetLayout::BindlessDescriptorSetLayout(const VulkanState &state,
                                                             const vk::ShaderStageFlags stage,
                                                             std::span<const BindingDescription> bindings) noexcept
{
  InplaceVector<vk::DescriptorSetLayoutBinding, desciptor_set_max_bindings> set_bindings;
  fill_binding(set_bindings, stage, bindings, resource_max_number_per_binding);

  InplaceVector<vk::DescriptorBindingFlags, desciptor_set_max_bindings> binding_flags;
  binding_flags.resize(bindings.size());
  std::fill(binding_flags.begin(), binding_flags.end(),
    vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind);
  vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info {
    .bindingCount = static_cast<uint32_t>(binding_flags.size()),
    .pBindingFlags = binding_flags.data(),
  };

  // Create descriptor set layout
  vk::DescriptorSetLayoutCreateInfo set_layout_create_info {
    .pNext = &binding_flags_create_info,
    .flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
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
  std::span<const mr::graphics::Shader::ResourceView> attachments) noexcept
{
  ASSERT(attachments.size() <= desciptor_set_max_bindings,
    "Max binding value is desciptor_set_max_bindings and all bindings must be unique");

  using WriteInfo =
    std::variant<vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;
  InplaceVector<WriteInfo, desciptor_set_max_bindings> write_infos(attachments.size());

  auto write_buffer = [&](const Buffer *buffer, vk::DescriptorBufferInfo &info) {
    info.buffer = buffer->buffer();
    info.range = buffer->byte_size();
    info.offset = 0;
  };
  auto write_conditional_buffer = [&](const ConditionalBuffer *buffer, vk::DescriptorBufferInfo &info) {
    write_buffer(buffer, info);
  };
  auto write_uniform_buffer = [&](const UniformBuffer *buffer, vk::DescriptorBufferInfo &info) {
    write_buffer(buffer, info);
  };
  auto write_storage_buffer = [&](const StorageBuffer *buffer, vk::DescriptorBufferInfo &info) {
    write_buffer(buffer, info);
  };
  auto write_texture = [&](const Texture *texture, vk::DescriptorImageInfo &info) {
    info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    info.imageView = texture->image().image_view();
    info.sampler = texture->sampler().sampler();
  };
  auto write_geometry_buffer = [&](const ColorAttachmentImage *image, vk::DescriptorImageInfo &info) {
    info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    info.imageView = image->image_view();
    info.sampler = nullptr;
  };
  auto write_default = [](auto res, auto) { ASSERT(false, "Unhandled attachment type", res); std::unreachable(); };

  std::array<bool, desciptor_set_max_bindings> used_bindings {}; // for validation

  for (const auto &[attachment_view, write_info] : std::views::zip(attachments, write_infos)) {
    const graphics::Shader::Resource &attachment = attachment_view;
    if (std::holds_alternative<const Texture *>(attachment) ||
        std::holds_alternative<const ColorAttachmentImage *>(attachment)) {
      write_info.emplace<vk::DescriptorImageInfo>();
    }

    std::visit(Overloads {write_uniform_buffer,
                          write_storage_buffer,
                          write_conditional_buffer,
                          write_texture,
                          write_geometry_buffer,
                          write_default},
               attachment,
               write_info);

    // Validate attachments types
    const auto &binding = _set_layout->bindings()[attachment_view.binding];
    ASSERT(binding.has_value(), "Binding doesn't appear in SetLayout create info",
      attachment_view.binding);
    ASSERT(binding.value() == get_descriptor_type(attachment),
      "Type of binding differ type of this binding in SetLayout create info",
      attachment_view.binding);
    ASSERT(!used_bindings[attachment_view.binding], "binding appears at least twice",
      attachment_view.binding);
    used_bindings[attachment_view.binding] = true;
  }

  static constexpr size_t max_descriptor_writes = 64;
  ASSERT(attachments.size() < max_descriptor_writes);
  InplaceVector<vk::WriteDescriptorSet, max_descriptor_writes> descriptor_writes;

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
    vk::DescriptorPoolSize {vk::DescriptorType::eStorageImage, 20},
    // vk::DescriptorPoolSize {vk::DescriptorType::eUniformBufferDynamic, 5},
    // vk::DescriptorPoolSize {vk::DescriptorType::eStorageBufferDynamic, 5},
  };

  if (auto pool = allocate_pool(default_sizes); pool.has_value()) {
    _pools.emplace_back(std::move(pool.value()));
  }

  static constexpr std::array bindless_default_sizes {
    vk::DescriptorPoolSize {vk::DescriptorType::eUniformBuffer,        resource_max_number_per_binding},
    vk::DescriptorPoolSize {vk::DescriptorType::eStorageBuffer,        resource_max_number_per_binding},
    vk::DescriptorPoolSize {vk::DescriptorType::eCombinedImageSampler, resource_max_number_per_binding},
    vk::DescriptorPoolSize {vk::DescriptorType::eStorageImage,         resource_max_number_per_binding},
  };
  auto bindless_pool = allocate_pool(bindless_default_sizes, true);
  ASSERT(bindless_pool.has_value(), "Error in allocating descriptor pool");
  _bindless_pool = std::move(bindless_pool.value());
}

std::optional<vk::UniqueDescriptorPool> mr::DescriptorAllocator::allocate_pool(
  std::span<const vk::DescriptorPoolSize> sizes, bool bindless) noexcept
{
  // Create descriptor pool
  vk::DescriptorPoolCreateInfo pool_create_info {
    .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
           | (bindless ? vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind
                       : vk::DescriptorPoolCreateFlags {}),
    .maxSets = 100, // TODO(dk6): This is must bw reworked
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
std::optional<mr::DescriptorSet> mr::DescriptorAllocator::allocate_set(
  DescriptorSetLayoutHandle set_layout) const noexcept
{
  return allocate_sets({&set_layout, 1}).transform(
    [](auto &&v) -> mr::DescriptorSet {
      return std::move(v[0]);
    });
}

// TODO: handle resizing pool vector
std::optional<mr::SmallVector<mr::DescriptorSet>> mr::DescriptorAllocator::allocate_sets(
  std::span<const DescriptorSetLayoutHandle> set_layouts) const noexcept
{
  static constexpr size_t max_descriptor_set_number = 64;
  ASSERT(set_layouts.size() < max_descriptor_set_number);
  InplaceVector<vk::DescriptorSetLayout, max_descriptor_set_number> layouts;

  SmallVector<DescriptorSet> sets;
  sets.reserve(set_layouts.size());

  for (const auto &layout : set_layouts) {
    layouts.emplace_back(layout->layout());
    sets.emplace_back(vk::DescriptorSet(), layout);
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

std::optional<mr::BindlessDescriptorSet> mr::DescriptorAllocator::allocate_bindless_set(
  BindlessDescriptorSetLayoutHandle set_layout) const noexcept
{
  auto vk_set_layout = set_layout->layout();
  vk::DescriptorSetAllocateInfo allocate_info {
    .descriptorPool = _bindless_pool.get(),
    .descriptorSetCount = 1,
    .pSetLayouts = &vk_set_layout,
  };

  auto &&[res_set, sets] = _state->device().allocateDescriptorSetsUnique(allocate_info);
  ASSERT(res_set == vk::Result::eSuccess);
  return BindlessDescriptorSet(*_state, std::move(sets.front()), set_layout);
}

void mr::DescriptorAllocator::reset() noexcept
{
  for (auto &pool : _pools) {
    _state->device().resetDescriptorPool(pool.get());
  }
}

// ============================================================================
// Bindless descriptor set functions
// ============================================================================

mr::BindlessDescriptorSet::BindlessDescriptorSet(const VulkanState &state,
                                                 vk::UniqueDescriptorSet set,
                                                 BindlessDescriptorSetLayoutHandle layout) noexcept
  : _state(&state), _set(std::move(set)), _set_layout(std::move(layout))
{
  struct StatInfo {
    uint32_t count = 0;
    uint32_t last_binding = -1;
  };
  boost::unordered_map<vk::DescriptorType, StatInfo> stats;
  for (auto [binding, type] : std::views::enumerate(_set_layout->bindings())) {
    if (type.has_value()) {
      auto &stat = stats[type.value()];
      stat.count++;
      stat.last_binding = binding;
    }
  }
  for (const auto &[type, stat] : stats) {
    if (stat.count == 1) {
      _unique_resources_types.emplace_back(TypeBindingPair {type, stat.last_binding});
    }
  }

  _resource_pools.resize(desciptor_set_max_bindings);
}

mr::graphics::Shader::ResourceView
mr::BindlessDescriptorSet::try_convert_view_to_resource(const mr::graphics::Shader::Resource &resource) const noexcept
{
  auto type = get_descriptor_type(resource);
  std::optional<uint32_t> binding = std::nullopt;
  for (const auto &[unique_type, type_binding] : _unique_resources_types) {
    if (type == unique_type) {
      binding = type_binding;
    }
  }
  ASSERT(binding.has_value(), "You try register resource without passing binding, but its type is ambigious", type);
  return mr::graphics::Shader::ResourceView(binding.value(), resource);
}

uint32_t mr::BindlessDescriptorSet::register_resource(const mr::graphics::Shader::ResourceView &resource_view) noexcept
{
  // Validate attacmnets type
  const auto &binding = _set_layout->bindings()[resource_view.binding];
  ASSERT(binding.has_value(), "Binding {} doesn't appear in SetLayout create info",
    resource_view.binding);
  ASSERT(binding.value() == get_descriptor_type(resource_view.res),
    "Type of binding {} differ type of this binding in SetLayout create info",
    resource_view.binding);

  ResourceInfo res_info;
  vk::WriteDescriptorSet write_info;
  uint32_t id = fill_resource(resource_view, res_info, write_info);
  // Maybe accumulating write_infos and calling updateDescriptorSets once per frame would probably be better
  _state->device().updateDescriptorSets(std::span {&write_info, 1}, {});
  return id;
}

uint32_t mr::BindlessDescriptorSet::register_resource(const mr::graphics::Shader::Resource &resource) noexcept
{
  ResourceInfo res_info;
  vk::WriteDescriptorSet write_info;
  uint32_t id = fill_resource(try_convert_view_to_resource(resource), res_info, write_info);
  // Maybe accumulating write_infos and calling updateDescriptorSets once per frame would probably be better
  _state->device().updateDescriptorSets(std::span {&write_info, 1}, {});
  return id;
}

mr::InplaceVector<uint32_t, mr::desciptor_set_max_bindings>
mr::BindlessDescriptorSet::register_resources(std::span<const mr::graphics::Shader::Resource> resources) noexcept
{
  ASSERT(resources.size() <= desciptor_set_max_bindings,
    "Max binding value is desciptor_set_max_bindings and all bindings must be unique");
  InplaceVector<ResourceInfo, desciptor_set_max_bindings> res_infos(resources.size());
  InplaceVector<vk::WriteDescriptorSet, desciptor_set_max_bindings> write_infos(resources.size());
  InplaceVector<uint32_t, mr::desciptor_set_max_bindings> ids;
  for (auto &&[resource, res_info, write_info] : std::views::zip(resources, res_infos, write_infos)) {
    ids.push_back(fill_resource(try_convert_view_to_resource(resource), res_info, write_info));
  }
  _state->device().updateDescriptorSets(write_infos, {});
  return ids;
}

template<typename T>
static std::uintptr_t get_resource_id(const T *res)
{
  return reinterpret_cast<std::uintptr_t>(res);
}

static std::uintptr_t get_resource_id(const mr::graphics::Shader::PyramidImageResource *res)
{
  // TODO(dk6): This can be bad for hash collisions
  //            Maybe it is better to require PyramidImageResource instance for each mip in calling code
  return reinterpret_cast<std::uintptr_t>((VkImageView)res->image->get_level(res->mip_level));
}

void mr::BindlessDescriptorSet::unregister_resource(const mr::graphics::Shader::Resource &resource) noexcept
{
  auto tex = [&](const Texture *tex) -> uint32_t {
    return get_resource_id(tex);
  };
  auto ubuf = [&](const UniformBuffer *buf) -> uint32_t {
    return get_resource_id(buf);
  };
  auto sbuf = [&](const StorageBuffer *buf) -> uint32_t {
    return get_resource_id(buf);
  };
  auto simg = [&](const StorageImage *img) -> uint32_t {
    return get_resource_id(img);
  };
  auto ssimg = [&](const graphics::Shader::SamplerStorageImage *img) -> uint32_t {
    return get_resource_id(img);
  };
  auto pimg = [&](const graphics::Shader::PyramidImageResource *img) -> uint32_t {
    return get_resource_id(img);
  };
  auto dimg = [&](const DepthImage *img) -> uint32_t {
    return get_resource_id(img);
  };
  auto other = [](auto &&unknown_res) -> uint32_t {
    ASSERT(false, "Unsupported in BindlessSet resource type", unknown_res);
    return 0;
  };
  auto resource_id = std::visit(Overloads {tex, ubuf, sbuf, simg, ssimg, pimg, dimg, other}, resource);

  uint32_t binding = _bindings_of_resources[resource_id];
  _resource_pools[binding].unregister(resource_id);
}

uint32_t mr::BindlessDescriptorSet::fill_resource(const mr::graphics::Shader::ResourceView &resource,
                                                  ResourceInfo &resource_info,
                                                  vk::WriteDescriptorSet &write_info) noexcept
{
  auto tex = [&](const Texture *tex) -> std::uintptr_t {
    fill_texture(tex, resource_info.emplace<vk::DescriptorImageInfo>());
    return get_resource_id(tex);
  };
  auto ubuf = [&](const UniformBuffer *buf) -> std::uintptr_t {
    fill_uniform_buffer(buf, resource_info.emplace<vk::DescriptorBufferInfo>());
    return get_resource_id(buf);
  };
  auto sbuf = [&](const StorageBuffer *buf) -> std::uintptr_t {
    fill_storage_buffer(buf, resource_info.emplace<vk::DescriptorBufferInfo>());
    return get_resource_id(buf);
  };
  auto simg = [&](const StorageImage *img) -> std::uintptr_t {
    fill_storage_image(img, resource_info.emplace<vk::DescriptorImageInfo>());
    return get_resource_id(img);
  };
  auto ssimg = [&](const graphics::Shader::SamplerStorageImage *img) -> std::uintptr_t {
    fill_sampled_storage_image(img, resource_info.emplace<vk::DescriptorImageInfo>());
    return get_resource_id(img);
  };
  auto pimg = [&](const graphics::Shader::PyramidImageResource *img) -> std::uintptr_t {
    fill_pyramid_image(img, resource_info.emplace<vk::DescriptorImageInfo>());
    return get_resource_id(img);
  };
  auto dimg = [&](const DepthImage *img) -> std::uintptr_t {
    fill_depth_image(img, resource_info.emplace<vk::DescriptorImageInfo>());
    return get_resource_id(img);
  };
  auto other = [](auto &&unknown_res) -> std::uintptr_t {
    ASSERT(false, "Unsupported in BindlessSet resource type", unknown_res);
    return {};
  };
  auto resource_id = std::visit(Overloads {tex, ubuf, sbuf, simg, ssimg, pimg, dimg, other}, resource.res);
  _bindings_of_resources[resource_id] = resource.binding;

  uint32_t index = _resource_pools[resource.binding].get_id(resource_id);
  write_info = vk::WriteDescriptorSet {
    .dstSet = _set.get(),
    .dstBinding = resource.binding,
    .dstArrayElement = index,
    .descriptorCount = 1,
    .descriptorType = get_descriptor_type(resource.res),
    .pImageInfo = std::get_if<vk::DescriptorImageInfo>(&resource_info),
    .pBufferInfo = std::get_if<vk::DescriptorBufferInfo>(&resource_info),
  };
  return index;
}

void mr::BindlessDescriptorSet::fill_texture(const Texture *texture,
                                             vk::DescriptorImageInfo &image_info) const noexcept
{
  image_info = vk::DescriptorImageInfo {
    .sampler = texture->sampler().sampler(),
    .imageView = texture->image().image_view(),
    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
  };
}

void mr::BindlessDescriptorSet::fill_uniform_buffer(const UniformBuffer *buffer,
                                                    vk::DescriptorBufferInfo &buffer_info) const noexcept
{
  buffer_info = vk::DescriptorBufferInfo {
    .buffer = buffer->buffer(),
    .offset = 0,
    .range = buffer->byte_size(),
  };
}

void mr::BindlessDescriptorSet::fill_storage_buffer(const StorageBuffer *buffer,
                                                    vk::DescriptorBufferInfo &buffer_info) const noexcept
{
  buffer_info = vk::DescriptorBufferInfo {
    .buffer = buffer->buffer(),
    .offset = 0,
    .range = buffer->byte_size(),
  };
}

void mr::BindlessDescriptorSet::fill_storage_image(const StorageImage *image,
                                                   vk::DescriptorImageInfo &image_info) const noexcept
{
  image_info = vk::DescriptorImageInfo {
    .imageView = image->image_view(),
    .imageLayout = vk::ImageLayout::eGeneral,
  };
}

void mr::BindlessDescriptorSet::fill_sampled_storage_image(
  const graphics::Shader::SamplerStorageImage *image,
  vk::DescriptorImageInfo &image_info) const noexcept
{
  image_info = vk::DescriptorImageInfo {
    .sampler = image->sampler->sampler(),
    .imageView = image->storage_image->image_view(),
    .imageLayout = vk::ImageLayout::eGeneral,
  };
}


void mr::BindlessDescriptorSet::fill_pyramid_image(const graphics::Shader::PyramidImageResource *image,
                                                   vk::DescriptorImageInfo &image_info) const noexcept
{
  image_info = vk::DescriptorImageInfo {
    .imageView = image->image->get_level(image->mip_level),
    .imageLayout = vk::ImageLayout::eGeneral,
  };
}

void mr::BindlessDescriptorSet::fill_depth_image(const DepthImage *image,
                                                 vk::DescriptorImageInfo &image_info) const noexcept
{
  image_info = vk::DescriptorImageInfo {
    .imageView = image->image_view(),
    // .imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal,
    .imageLayout = vk::ImageLayout::eGeneral,
  };
}

// ============================================================================
// Resource pool data for bindless descriptor set
// ============================================================================

mr::BindlessDescriptorSet::ResourcePoolData::ResourcePoolData(uint32_t max_resource_number) noexcept
  : max_number(max_resource_number) {}

mr::BindlessDescriptorSet::ResourcePoolData &
mr::BindlessDescriptorSet::ResourcePoolData::operator=(ResourcePoolData &&other) noexcept
{
  free_ids = std::move(other.free_ids);
  usage = std::move(other.usage);
  max_number = other.max_number;
  current_id = other.current_id.load();
  return *this;
}

mr::BindlessDescriptorSet::ResourcePoolData::ResourcePoolData(ResourcePoolData &&other) noexcept
{
  *this = std::move(other);
}

static uint32_t pop_back_vector(mr::InplaceVector<uint32_t, mr::resource_max_number_per_binding> &v)
{
  uint32_t res = v.back();
  v.pop_back();
  return res;
}

uint32_t mr::BindlessDescriptorSet::ResourcePoolData::get_id(std::uintptr_t resource) noexcept
{
  std::lock_guard lock(mutex);

  // Check if resource was already registered
  if (auto res_iter = usage.find(resource); res_iter != usage.end()) {
    auto &res_stat = res_iter->second;
    res_stat.usage_count++;
    return res_stat.id;
  }

  // Find new id for resource
  uint32_t id = free_ids.empty() ? current_id++ : pop_back_vector(free_ids);
  ASSERT(id < max_number, "Not enough space for allocate index descriptor set. "
    "To fix this you can increase 'resource_max_number_per_binding' in descriptor.hpp");
  usage[resource] = ResourceStat {
    .id = id,
    .usage_count = 1,
  };
  return id;
}

void mr::BindlessDescriptorSet::ResourcePoolData::unregister(std::uintptr_t resource) noexcept
{
  std::lock_guard lock(mutex);

  auto &stat = usage[resource];
  stat.usage_count--;
  if (stat.usage_count == 0) {
    free_ids.push_back(stat.id);
    usage.erase(resource);
  }
}

