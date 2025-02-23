#ifndef __manager_hpp_
#define __manager_hpp_

#include "pch.hpp"
#include "resource.hpp"

namespace mr {

  struct UnnamedTag {};
  constexpr inline UnnamedTag unnamed;

  template<typename ResourceT>
  class ResourceManager {
  public:
    static_assert(Resource<ResourceT>, "ResourceT does not satisfy Resource concept");

    using HandleT = Handle<ResourceT>;
    using ResourceMapT = std::unordered_map<std::string, std::weak_ptr<ResourceT>>;

    /**
     * @brief Retrieves the singleton instance of ResourceManager.
     *
     * This function returns a reference to the unique ResourceManager instance. It employs a static
     * local variable to ensure that only one instance exists throughout the application.
     *
     * @return ResourceManager& A reference to the singleton ResourceManager instance.
     */
    static ResourceManager & get() noexcept
    {
      static ResourceManager manager;
      return manager;
    }

    template<typename... Args>
    /**
     * @brief Creates and registers a new resource with the specified name.
     *
     * Constructs a new instance of ResourceT using the provided arguments and registers it under the given name.
     * The resource is stored internally and returned as a handle for future access.
     *
     * @param name The unique identifier for the resource.
     * @param args Additional arguments forwarded to ResourceT's constructor.
     * @return HandleT A handle to the newly created resource.
     */
    HandleT create(std::string name, Args &&...args)
    {
      HandleT resource = std::make_shared<ResourceT>(std::forward<Args>(args)...);
      _resources[std::move(name)] = resource;
      return resource;
    }

    template<typename... Args>
    /**
     * @brief Creates a new resource with an automatically generated unique name.
     *
     * This overload creates a resource without a user-provided name. A unique name is generated using
     * an internal static counter, and the provided arguments are perfectly forwarded to the resource's
     * constructor.
     *
     * @tparam Args Types of additional arguments for constructing the resource.
     * @param UnnamedTag Dummy parameter used to select the unnamed resource creation overload.
     * @param args Additional arguments forwarded to the resource's constructor.
     *
     * @return HandleT A handle to the newly created resource.
     *
     * @note The generated resource name is a string representation of an incrementing counter.
     */
    HandleT create(UnnamedTag, Args &&...args)
    {
      static size_t unnamed_id = 0;
      return create(std::to_string(unnamed_id++), std::forward<Args>(args)...);
    }

    /**
     * @brief Retrieves a resource by its name.
     *
     * Searches the internal resource map for an entry matching the provided name. If the resource exists,
     * the function returns a locked pointer to it; otherwise, it returns a null pointer.
     *
     * @param name The unique identifier of the resource.
     * @return HandleT A pointer to the resource if found, or a null pointer if not.
     */
    HandleT find(const std::string &name)
    {
      if (auto it = _resources.find(name); it != _resources.end())
        return it->second.lock();

      return nullptr;
    }

  private:
    /**
     * @brief Constructs a ResourceManager instance.
     *
     * Reserves capacity for 64 resource entries in the internal container to optimize memory allocation.
     */
    ResourceManager() noexcept
    {
      _resources.reserve(64);
    }

    ResourceMapT _resources;
  };

} // namepsce mr

#endif // __manager_hpp_
