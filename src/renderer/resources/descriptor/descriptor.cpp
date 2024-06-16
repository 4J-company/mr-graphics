#include "resources/descriptor/descriptor.hpp"

void mr::Descriptor::apply() {}

mr::Descriptor::Descriptor(const VulkanState &state, Pipeline *pipeline,
                           const std::vector<Attachment::Data> &attachments,
                           uint set_number)
    : _set_number(set_number)
    , _set_layout(pipeline->set_layout(_set_number))
{
  create_descriptor_pool(state, attachments);
  update_all_attachments(state, attachments);
}

void mr::Descriptor::update_all_attachments(
  const VulkanState &state, const std::vector<Attachment::Data> &attachments)
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

  for (const auto &[attachment, write_info] :
       std::views::zip(attachments, write_infos)) {
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

void mr::Descriptor::create_descriptor_pool(
  const VulkanState &state, const std::vector<Attachment::Data> &attachments)
{
  // Check attachments existing
  if (attachments.empty()) {
    return;
  }

  std::vector<vk::DescriptorPoolSize> pool_sizes(attachments.size());
  for (const auto &[attachment, pool_size] :
       std::views::zip(attachments, pool_sizes)) {
    pool_size.descriptorCount = 1;
    pool_size.type = get_descriptor_type(attachment);
  }

  vk::DescriptorPoolCreateInfo pool_create_info {
    .maxSets = 1,
    .poolSizeCount = static_cast<uint>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data(),
  };

  _pool = state.device().createDescriptorPoolUnique(pool_create_info).value;

  vk::DescriptorSetAllocateInfo descriptor_alloc_info {
    .descriptorPool = _pool.get(),
    .descriptorSetCount = 1,
    .pSetLayouts = &_set_layout,
  };

  _set = state.device().allocateDescriptorSets(descriptor_alloc_info).value[0];
}

vk::DescriptorType
mr::get_descriptor_type(const Descriptor::Attachment::Data &attachment) noexcept
{
  using enum vk::DescriptorType;
  static const std::array types {
    eUniformBuffer,
    eStorageBuffer,
    eCombinedImageSampler,
    eInputAttachment,
  };

  return types[attachment.index()];
}
