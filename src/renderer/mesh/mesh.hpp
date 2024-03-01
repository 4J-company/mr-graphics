#ifndef __mesh_cpp_
#define __mesh_cpp_

#include "pch.hpp"
#include "resources/resources.hpp"

#include <any>

#include "attribute_types.hpp"

namespace mr
{
  struct Boundbox
  {
    // vec4 min, max;
  };

  class Mesh 
  {
  private:
    Buffer _vbuf;
    Buffer _ibuf;

    size_t _element_count = 0;

    std::atomic<int> _instance_count = 0;

    // mr::Boundbox _boundbox;

  public:
    Mesh() = default;

    Mesh(std::span<PositionType> positions,
         std::span<FaceType> faces,
         std::span<ColorType> colors,
         std::span<TexCoordType> uvs,
         std::span<NormalType> normals,
         std::span<NormalType> tangents,
         std::span<NormalType> bitangent,
         std::span<BoneType> bones,
         BoundboxType bbox
        );

    // copy semantics
    Mesh(const Mesh &other) noexcept = delete;
    Mesh & operator=(const Mesh &other) noexcept = delete;

    // move semantics
    Mesh(Mesh &&other) noexcept {
      _vbuf = std::move(other._vbuf);
      _ibuf = std::move(other._ibuf);
      _element_count = std::move(other._element_count);
      _instance_count = std::move(other._instance_count.load());
    }
    Mesh & operator=(Mesh &&other) noexcept {
      _vbuf = std::move(other._vbuf);
      _ibuf = std::move(other._ibuf);
      _element_count = std::move(other._element_count);
      _instance_count = std::move(other._instance_count.load());
    }
  };
};

#endif // __mesh_cpp_
