#ifndef __mesh_cpp_
#define __mesh_cpp_

#include "pch.hpp"
#include "resources/resources.hpp"

namespace mr
{
  struct Boundbox
  {
    vec4 min, max;
  };

  class Mesh 
  {
  private:
    Buffer _vbuf;
    Buffer _ibuf;

    size_t _element_count = 0;

    std::atomic<int> _instance_count = 0;

    mr::Boundbox _boundbox;

  public:
    Mesh() = default;
    Mesh(Mesh &&other) noexcept = delete;
    Mesh & operator=(Mesh &&other) noexcept = delete;
    Mesh(const Mesh &other) noexcept = delete;
    Mesh & operator=(const Mesh &other) noexcept = delete;
  };
};

#endif // __mesh_cpp_
