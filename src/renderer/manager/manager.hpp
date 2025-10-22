#ifndef __manager_hpp_
#define __manager_hpp_

#include "pch.hpp"
#include "resource.hpp"

namespace mr {
inline namespace graphics {
  struct UnnamedTag {};
  constexpr inline UnnamedTag unnamed;

  template<typename ResourceT>
  class ResourceManager {
  public:
    static_assert(Resource<ResourceT>, "ResourceT does not satisfy Resource concept");

    using HandleT = Handle<ResourceT>;
    using ResourceMapT = boost::unordered_map<std::string, std::weak_ptr<ResourceT>>;

    // tmp singleton
    static ResourceManager & get() noexcept
    {
      static ResourceManager manager;
      return manager;
    }

    template<typename... Args>
    HandleT create(std::string name, Args &&...args)
    {
      HandleT resource = std::make_shared<ResourceT>(std::forward<Args>(args)...);
      _resources[std::move(name)] = resource;
      return resource;
    }

    template<typename... Args>
    HandleT create(UnnamedTag, Args &&...args)
    {
      static size_t unnamed_id = 0;
      return create(std::to_string(unnamed_id++), std::forward<Args>(args)...);
    }

    HandleT find(const std::string &name)
    {
      if (auto it = _resources.find(name); it != _resources.end())
        return it->second.lock();

      return nullptr;
    }

  private:
    ResourceManager() noexcept
    {
      _resources.reserve(64);
    }

    ResourceMapT _resources;
  };
}
} // namepsce mr

#endif // __manager_hpp_
