#if !defined(__vulkan_application_hpp_)
#define __vulkan_application_hpp_

#include "pch.hpp"

namespace mr {
  class VulkanState {
      friend class Application;

    private:
      vk::Instance _instance;
      vk::Device _device;
      vk::PhysicalDevice _phys_device;
      vk::Queue _queue;
      vk::PipelineCache _pipeline_cache;

    public:
      void create_pipeline_cache()
      {
        // retrieve pipeline cache data from previous runs
        size_t cache_size = 0;
        std::vector<char> cache_data;
        const auto cache_path = "bin/cache/pipeline";
        std::fstream cache_file(
          cache_path, std::ios::in | std::ios::out | std::ios::binary);

        if (cache_file.is_open()) {
          cache_file.seekg(0, std::ios::end);
          cache_size = cache_file.tellg();
          cache_data.reserve(cache_size);
          cache_file.seekg(0, std::ios::beg);
          cache_file.read(cache_data.data(), cache_size);
          cache_file.clear();
          // TODO: maybe cache validation is needed (vk::PipelineCacheHeaderVersionOne)
        }
        else {
          cache_file.open(cache_path, std::ios::out | std::ios::binary);
        }

        vk::PipelineCacheCreateInfo create_info {
          .initialDataSize = cache_size, .pInitialData = cache_data.data()};
        _pipeline_cache = _device.createPipelineCache(create_info).value;

        if (!cache_file.is_open()) {
          return;
        }

        // overwrite pipeline cache
        auto result =
          _device.getPipelineCacheData(_pipeline_cache, &cache_size, nullptr);
        if (result == vk::Result::eSuccess) {
          return;
        }

        cache_data.reserve(cache_size);
        result = _device.getPipelineCacheData(
          _pipeline_cache, &cache_size, cache_data.data());
        if (result == vk::Result::eSuccess) {
          return;
        }

        cache_file.seekp(0, std::ios::beg);
        cache_file.write(cache_data.data(), cache_size);
      }

      vk::Instance instance() const { return _instance; }

      vk::PhysicalDevice phys_device() const { return _phys_device; }

      vk::Device device() const { return _device; }

      vk::Queue queue() const { return _queue; }

      vk::PipelineCache pipeline_cache() const { return _pipeline_cache; }
  };
} // namespace mr

#endif // __vulkan_application_hpp_
