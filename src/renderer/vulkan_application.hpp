#if !defined(__vulkan_application_hpp_)
  #define __vulkan_application_hpp_

  #include "pch.hpp"

namespace mr
{
  class VulkanState
  {
    friend class Application;

  private:
    vk::Instance _instance;
    vk::Device _device;
    vk::PhysicalDevice _phys_device;
    vk::RenderPass _render_pass;
    vk::Queue _queue;
    vk::PipelineCache _pipeline_cache;

  public:
    VulkanState() = default;

    void create_pipeline_cache()
    {
      // retrieve pipeline cache data from previous runs
      size_t cache_size = 0;
      char *cache_data = nullptr;
      std::fstream cache_file("bin/cache/pipeline", std::ios::in | std::ios::out | std::ios::binary);
      if (cache_file)
      {
        cache_file.seekg(0, std::ios::end);
        cache_size = cache_file.tellg();
        cache_data = new char[cache_size];
        cache_file.seekg(0, std::ios::beg);
        cache_file.read(cache_data, cache_size);
        // TODO: maybe cache validation is needed (vk::PipelineCacheHeaderVersionOne)
      }
      else
        cache_file.open("bin/cache/pipeline", std::ios::out | std::ios::binary);

      // create pipleline cache
      vk::PipelineCacheCreateInfo pipeline_cache_create_info {.initialDataSize = cache_size,
                                                              .pInitialData = cache_data};
      _pipeline_cache = _device.createPipelineCache(pipeline_cache_create_info).value;
      if (cache_data != nullptr)
        delete[] cache_data;

      // save pipeline cache (actual) for next runs
      auto result = _device.getPipelineCacheData(_pipeline_cache, &cache_size, nullptr);
      assert(result == vk::Result::eSuccess);
      char *cache_save_data = new char[cache_size]; // TODO: maybe using cache_data buffer instead
      result = _device.getPipelineCacheData(_pipeline_cache, &cache_size, cache_save_data);
      assert(result == vk::Result::eSuccess);
      if (cache_file && cache_save_data != nullptr)
      {
        cache_file.seekp(0, std::ios::beg);
        cache_file.write(cache_save_data, cache_size);
      }
      if (cache_save_data != nullptr)
        delete[] cache_save_data;
    }

    vk::Instance instance() const { return _instance; }
    vk::PhysicalDevice phys_device() const { return _phys_device; }
    vk::Device device() const { return _device; }
    vk::RenderPass render_pass() const { return _render_pass; }
    vk::Queue queue() const { return _queue; }
    vk::PipelineCache pipeline_cache() const { return _pipeline_cache; }
  };
} // namespace mr

#endif // __vulkan_application_hpp_
