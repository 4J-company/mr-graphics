#include "CrossWindow/Common/Event.h"
#if !defined(__texture_hpp_)
  #define __texture_hpp_

  #include "resources/texture/sampler/sampler.hpp"

namespace ter
{
  class Texture
  {
  private:
    Image _image;
    Sampler _sampler;

  public:
    Texture() = default;
    Texture(std::string_view filename);
    ~Texture();

    Texture(Texture &&other) noexcept = default;
    Texture &operator=(Texture &&other) noexcept = default;
  };
} // namespace ter

#endif // __texture_hpp_
