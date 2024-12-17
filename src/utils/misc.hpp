#pragma once

#include "pch.hpp"

namespace mr {
  template <typename... Ts>
    struct Overloads : Ts... {
      using Ts::operator()...;
  };

  template <typename T>
    concept ExtentLike = requires(T t) { t.width; t.height; };

  struct Extent {
    using ValueT = uint;

    ValueT width {};
    ValueT height {};

    Extent() = default;

    constexpr Extent(ValueT width_, ValueT height_) noexcept
      : width{height_}, height{height_} {}

    // converting to/from other extents
    template <ExtentLike Other>
      constexpr Extent(const Other &extent) noexcept
        : width{extent.width}, height{extent.height} {}

    template <ExtentLike Other>
      constexpr Extent & operator =(const Other &extent) noexcept
      {
        width = extent.width, height = extent.height;
        return *this;
      }

    template <ExtentLike Other>
      constexpr operator Other() const noexcept
      {
        return { .width = width, .height = height };
      }
  };

  class CacheFile {
  public:
    CacheFile() = default;
    CacheFile(CacheFile &&) = default;
    CacheFile & operator=(CacheFile &&) = default;

    CacheFile(std::fs::path file_path)
    {
      open(file_path);
    }

    bool open(std::fs::path file_path)
    {
      if (not _bytes.empty()) {
        return false;
      }

      _file_path = std::move(file_path);
      std::ifstream file(_file_path, std::ios::binary);
      if (not file.is_open()) {
#ifndef NDEBUG
        std::cerr << "Cannot open file " << _file_path << "\n\t Error: " << std::strerror(errno) << "\n\n";
#endif
        return false;
      }

      size_t data_size;
      file.seekg(0, std::ios::end);
      data_size = file.tellg();

      _bytes.resize(data_size);
      file.seekg(0, std::ios::beg);
      file.read(reinterpret_cast<char *>(_bytes.data()), data_size);

      return true;
    }

    ~CacheFile()
    {
      if (_bytes.empty())
        return;

      std::ofstream file(_file_path, std::ios::trunc | std::ios::binary);
      if (not file.is_open()) {
        return;
      }

      file.write(reinterpret_cast<char *>(_bytes.data()), _bytes.size());
    }

    const std::vector<byte> & bytes() const noexcept { return _bytes; }
    std::vector<byte> & bytes() noexcept { return _bytes; }

  private:
    std::fs::path _file_path;
    std::vector<byte> _bytes;
  };
} // namespace mr
