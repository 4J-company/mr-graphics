#include "resources/descriptor/descriptor.hpp"

void mr::Descriptor::apply() {}

mr::Descriptor::Descriptor(const VulkanState &state, Pipeline *pipeline, const std::vector<DescriptorAttachment> &attachments, uint set_number)
  : _set_number(set_number), _set_layout(pipeline->set_layout(_set_number))
{
  create_descriptor_pool(state, attachments);
  update_all_attachments(state, attachments);
}

void mr::Descriptor::update_all_attachments(const VulkanState &state, const std::vector<DescriptorAttachment> &attachments)
{
  // Stucture to store attachments descriptor data
  struct attach_write
  {
    vk::DescriptorBufferInfo buffer_info;
    vk::DescriptorImageInfo image_info;
  };

  std::vector<attach_write> _attach_write(attachments.size());
  for (uint i = 0; i < attachments.size(); i++)
  {
    if (attachments[i].texture != nullptr)
    {
      _attach_write[i].image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
      _attach_write[i].image_info.imageView = attachments[i].texture->image().image_view();
      _attach_write[i].image_info.sampler = attachments[i].texture->sampler().sampler();
    }
    if (attachments[i].uniform_buffer != nullptr)
    {
      _attach_write[i].buffer_info.buffer = attachments[i].uniform_buffer->buffer();
      _attach_write[i].buffer_info.range = attachments[i].uniform_buffer->size();
      _attach_write[i].buffer_info.offset = 0;
    }
  }

  std::vector<vk::WriteDescriptorSet> descriptor_writes(attachments.size());
  for (uint i = 0; i < attachments.size(); i++)
  {
    descriptor_writes[i] = 
      vk::WriteDescriptorSet
      {
        .dstSet = _set,
        .dstBinding = i,
        .dstArrayElement = 0,
        .descriptorCount = 1,
      };

    if (attachments[i].texture != nullptr)
    {
      descriptor_writes[i].descriptorType = vk::DescriptorType::eCombinedImageSampler;
      descriptor_writes[i].pImageInfo = &_attach_write[i].image_info;
    }
    else if (attachments[i].uniform_buffer != nullptr)
    {
      descriptor_writes[i].descriptorType = vk::DescriptorType::eUniformBuffer;
      descriptor_writes[i].pBufferInfo = &_attach_write[i].buffer_info;
    }
  }

  state.device().updateDescriptorSets(descriptor_writes, {});
}

void mr::Descriptor::create_descriptor_pool(const VulkanState &state, const std::vector<DescriptorAttachment> &attachments)
{
  // Check attachments existing
  if (attachments.empty())
    return;

  std::vector<vk::DescriptorPoolSize> pool_sizes(attachments.size());
  for (uint i = 0; i < attachments.size(); i++)
  {
    pool_sizes[i].descriptorCount = 1;

    if (attachments[i].uniform_buffer != nullptr)
      pool_sizes[i].type = vk::DescriptorType::eUniformBuffer; // Fill binding as UBO
    else if (attachments[i].storage_buffer != nullptr)
      pool_sizes[i].type = vk::DescriptorType::eStorageBuffer; // Fill binding as SSBO
    else if (attachments[i].texture != nullptr)
      pool_sizes[i].type = vk::DescriptorType::eCombinedImageSampler; // Fill binding as texture
    else
      assert(false); // bad attachment
  }

  vk::DescriptorPoolCreateInfo pool_create_info
  {
    .maxSets = 1,
    .poolSizeCount = static_cast<uint>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data(),
  };

  _pool = state.device().createDescriptorPoolUnique(pool_create_info).value;

  vk::DescriptorSetAllocateInfo descriptor_alloc_info
  {
    .descriptorPool = _pool.get(),
    .descriptorSetCount = 1,
    .pSetLayouts = &_set_layout,
  };

  _set = state.device().allocateDescriptorSets(descriptor_alloc_info).value[0];
}
