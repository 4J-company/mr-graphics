#if !defined(__shader_hpp_)
#define __shader_hpp_

#include "pch.hpp"

#include "vulkan_application.hpp"

namespace mr {
  class UniformBuffer;
  class StorageBuffer;
  class Texture;
  class Image;

  class Shader {
    public:
      using Resource = std::variant<UniformBuffer *, StorageBuffer *, Texture *, Image *>;

      // TODO: consider RT shaders from extensions;
      enum struct Stage {
        Compute  = 0,
        Vertex   = 1,
        Control  = 2,
        Evaluate = 3,
        Geometry = 4,
        Fragment = 5,
      };

      struct ResourceView {
        uint32_t set;
        uint32_t binding;
        Resource res;

        operator const Resource&() const { return res; }
      };

      struct ReflectedBinding {
        uint32_t binding;
        uint32_t set;
        vk::DescriptorType type;
        uint32_t descriptor_count = 1;
        vk::ShaderStageFlagBits stages;
      };
      using ReflectedDescriptorSet = std::vector<std::optional<ReflectedBinding>>;

    private:
      static inline const size_t max_shader_modules = 6;

      std::filesystem::path _path;
      std::string _name;
      std::array<vk::UniqueShaderModule, max_shader_modules> _modules;
      std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> _stages;
      std::atomic<uint> _num_of_loaded_shaders;

      std::array<std::atomic<bool>, max_shader_modules> _stage_reloaded {};
      std::atomic<bool> _recompiled = false;
      std::atomic<bool> _reloaded = false;

      std::vector<ReflectedDescriptorSet> _reflected_sets;
      std::vector<vk::VertexInputAttributeDescription> _reflected_attributes;

    public:
      Shader() = default;

      Shader(const VulkanState &state, std::string_view filename);

      // move semantics
      Shader(Shader &&other) noexcept
          : _path(std::move(other._path))
          , _modules(std::move(other._modules))
          , _stages(std::move(other._stages))
          , _reflected_sets(other._reflected_sets)
          , _reflected_attributes(other._reflected_attributes)
          , _num_of_loaded_shaders(other._num_of_loaded_shaders.load())
      {
      }

      Shader &operator=(Shader &&other) noexcept
      {
        // do not need a self check
        _path = std::move(other._path);
        _modules = std::move(other._modules);
        _stages = std::move(other._stages);
        _num_of_loaded_shaders = other._num_of_loaded_shaders.load();
        _reflected_attributes = std::move(other._reflected_attributes);
        _reflected_sets = std::move(other._reflected_sets);
        return *this;
      }

    private:
      // compile sources
      void compile(Stage stage) const noexcept;

      // load sources
      std::optional<std::vector<char>> load(Stage stage) noexcept;

      bool _validate_stage(Stage stage, bool present)  const noexcept;

      void recompile(const VulkanState &state);
      void reload(const VulkanState &state);

      bool check_stage_exist(int stage);
      bool check_need_recompile(int stage);

      void reflect_metadata(std::span<char> spv_data, Stage stage);

    public:
      const std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> &
      get_stages() const { return _stages; }

      std::array<vk::PipelineShaderStageCreateInfo, max_shader_modules> &
      get_stages() { return _stages; }

      uint stage_number() const noexcept { return _num_of_loaded_shaders; }

      std::string_view name() { return _name; }
      bool reloaded() { return _reloaded.load(); }
      bool claer_reloaded() { return _reloaded = false; }

      std::vector<vk::VertexInputAttributeDescription> &
      attributes() { return _reflected_attributes; }

      static void hot_recompile_shaders(const VulkanState &state,
                                        std::map<std::string, Shader> &shaders,
                                        std::mutex &mutex) noexcept;
      static void hot_reload_shaders(const VulkanState &state,
                                     std::map<std::string, Shader> &shaders,
                                     std::mutex &mutex) noexcept;
  };

  constexpr vk::ShaderStageFlagBits get_stage_flags(std::integral auto stage) noexcept
  {
    static constexpr std::array stage_bits {
      vk::ShaderStageFlagBits::eCompute,
      vk::ShaderStageFlagBits::eVertex,
      vk::ShaderStageFlagBits::eTessellationControl,
      vk::ShaderStageFlagBits::eTessellationEvaluation,
      vk::ShaderStageFlagBits::eGeometry,
      vk::ShaderStageFlagBits::eFragment
    };
    assert(stage < stage_bits.size());

    return stage_bits[stage];
  }

  constexpr vk::ShaderStageFlagBits get_stage_flags(Shader::Stage stage) noexcept
  {
    return get_stage_flags(std::to_underlying(stage));
  }

  constexpr const char * get_stage_name(std::integral auto stage) noexcept
  {
    static constexpr std::array shader_type_names {
      "comp",
      "vert",
      "tesc",
      "tese",
      "geom",
      "frag",
    };
    assert(stage < shader_type_names.size());
    return shader_type_names[stage];
  }

  constexpr const char * get_stage_name(Shader::Stage stage) noexcept
  {
    return get_stage_name(std::to_underlying(stage));
  }
} // namespace mr

#endif // __shader_hpp_
