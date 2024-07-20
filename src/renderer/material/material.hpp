#ifndef __material_cpp_
#define __material_cpp_

#include "pch.hpp"
#include "resources/resources.hpp"

namespace mr {
  // Blinn-Phong material
  class Material {
  private:
    std::vector<Texture> _ambient;
    std::vector<Texture> _diffuse;
    std::vector<Texture> _specular;

  public:
    Material() noexcept = default;
    Material(std::vector<Texture> ambient,
             std::vector<Texture> diffuse,
             std::vector<Texture> specular)
    : _ambient(std::move(ambient))
    , _diffuse(std::move(diffuse))
    , _specular(std::move(specular))
    {}
  };
}

#endif __material_cpp_
