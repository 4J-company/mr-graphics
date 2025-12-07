#ifndef __MR_SHADER_HPP_
#define __MR_SHADER_HPP_

#include "pch.hpp"

#include "manager/resource.hpp"
#include <boost/unordered/unordered_flat_map.hpp>

#include "vulkan_state.hpp"

namespace mr {
inline namespace graphics {
  class UniformBuffer;
  class StorageBuffer;
  class Texture;
  class Image;

  class Shader : public ResourceBase<Shader> {
  public:
    static inline constexpr size_t max_shader_modules = 8;

    using Resource = std::variant<const UniformBuffer *, const StorageBuffer *, const Texture *, const Image *, const ConditionalBuffer*>;

    struct ResourceView {
      uint32_t binding;
      Resource res;

      operator const Resource&() const { return res; }
    };

    using DefineMap = boost::unordered_flat_map<std::string, std::string>;
    template <typename T> using StageWiseVector = InplaceVector<T, max_shader_modules>;

  private:
    StageWiseVector<vk::UniqueShaderModule> _modules;
    StageWiseVector<vk::PipelineShaderStageCreateInfo> _stages;

    // `hashid`s of 2 Shader objects are equal only
    //     if both of them were created with equal parameters (path and defines)
    uint32_t hashid;

  public:
    Shader() = default;

    Shader(const VulkanState &state, std::filesystem::path path, const DefineMap &define_map);
    Shader(Shader &&other) noexcept = default;
    Shader &operator=(Shader &&other) noexcept = default;
    Shader(const Shader &other) noexcept = delete;
    Shader &operator=(const Shader &other) noexcept = delete;

    Shader & from_glsl_directory(const VulkanState &state, std::filesystem::path glsl_directory, const DefineMap &define_map);
    Shader & from_slang_file(const VulkanState &state, std::filesystem::path slang_file);

    decltype(auto) stages() const noexcept { return _stages; }
    decltype(auto) stages() noexcept { return _stages; }

    uint32_t id() const noexcept { return hashid; }
  };

  MR_DECLARE_HANDLE(Shader)
}
} // namespace mr

#endif // __MR_SHADER_HPP_
