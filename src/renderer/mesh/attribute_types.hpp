#ifndef __MR_ATTRIBUTE_TYPES_HPP_
#define __MR_ATTRIBUTE_TYPES_HPP_

using PositionType = mr::Vec3f;
using IndexType = int;
using NormalType = mr::Vec3f;
using TexCoordType = mr::Vec2f;
using ColorType = mr::Vec4f;
using BoneType = int;
using BoundboxType = mr::AABBf;

namespace mr {
  constexpr static uint32_t vertex_buffers_number = 2;
  constexpr static uint32_t position_bytes_size = sizeof(mr::Position);
  constexpr static uint32_t attributes_bytes_size = sizeof(mr::VertexAttributes);

  struct IndexBufferDescription {
    VkDeviceSize offset;
    uint32_t elements_count;
  };

  struct VertexBufferDescription {
    VkDeviceSize offset;
  };

  using VertexBuffersArray = SmallVector<VertexBufferDescription, vertex_buffers_number>;
} // end of 'mr' namespace

#endif // __MR_ATTRIBUTE_TYPES_HPP_
