#ifndef __misc_hpp_
#define __misc_hpp_

#include "pch.hpp"
#include "utils/log.hpp"

namespace mr {
  template <typename... Ts>
    struct Overloads : Ts... {
      using Ts::operator()...;
  };

  template<std::integral To = size_t, typename Enum> requires std::is_enum_v<Enum>
    constexpr To enum_cast(Enum value) noexcept { return static_cast<To>(value); }

  template<typename Enum, std::integral From> requires std::is_enum_v<Enum>
    constexpr Enum enum_cast(From value) noexcept { return static_cast<Enum>(value); }
  
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
      open_or_create(std::move(file_path));
    }

    bool open(std::fs::path file_path)
    {
      if (not _bytes.empty()) {
        return false;
      }

      _file_path = std::move(file_path);
      std::ifstream file(_file_path, std::ios::binary);
      if (not file.is_open()) {
        MR_ERROR("Cannot open file {}. {}\n", _file_path.string(), std::strerror(errno));
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

    bool open_or_create(std::fs::path file_path)
    {
      if (not open(std::move(file_path))) {
        MR_INFO("Creating file instead...");

        std::error_code error;
        std::fs::create_directory(_file_path.parent_path(), error);

        std::ofstream file(_file_path, std::ios::binary);
        if (not file.is_open()) {
          MR_ERROR("Cannot create file {}. {}\n", _file_path.string(), std::strerror(errno));
          return false;
        }
      }

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

#endif // __misc_hpp_
