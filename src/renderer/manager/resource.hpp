#ifndef __resource_hpp_
#define __resource_hpp_

namespace mr {

  template<typename ResourceT>
  class ResourceBase;

  template<typename T>
  concept Resource = std::derived_from<T, ResourceBase<T>>;

  template<Resource ResourceT>
  using Handle = std::shared_ptr<ResourceT>;

  template<typename ResourceT>
  class ResourceManager;

  template<typename ResourceT>
  class ResourceBase : public std::enable_shared_from_this<ResourceT> {
  protected:
    /**
 * @brief Default constructor for ResourceBase.
 *
 * This constructor is defaulted and protected to ensure that ResourceBase is only instantiated
 * as a base class for derived resource types, supporting shared pointer semantics.
 */
ResourceBase() = default;

    friend class ResourceManager<ResourceT>;
  };

}

#define MR_DECLARE_HANDLE(ResourceT) using ResourceT##Handle = Handle<ResourceT>;

#endif // __resource_hpp_
