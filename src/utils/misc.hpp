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
    /**
 * @brief Converts an enum value to a specified type.
 *
 * This function performs a compile-time conversion of an enum value to another type using
 * static_cast, typically for safely obtaining the underlying integral value.
 *
 * @tparam To The target type to convert the enum value to.
 * @tparam Enum The enum type of the input value.
 * @param value The enum value to convert.
 * @return The converted value as type To.
 */
constexpr To enum_cast(Enum value) noexcept { return static_cast<To>(value); }

  template<typename Enum, std::integral From> requires std::is_enum_v<Enum>
    /**
 * @brief Converts a value to a specified enumeration type.
 *
 * This function casts a given value to the enumeration type provided as the template parameter
 * using static_cast. The conversion is performed at compile time if possible, and the function is
 * guaranteed not to throw exceptions.
 *
 * @tparam Enum The enumeration type to convert to.
 * @tparam From The type of the value to convert, typically an integral type.
 * @param value The value to convert.
 * @return The corresponding value of type Enum.
 */
constexpr Enum enum_cast(From value) noexcept { return static_cast<Enum>(value); }
  
  template <typename T>
    concept ExtentLike = requires(T t) { t.width; t.height; };

  struct Extent {
    using ValueT = uint;

    ValueT width {};
    ValueT height {};

    Extent() = default;

    /**
       * @brief Constructs an Extent instance with the specified dimensions.
       *
       * Initializes the width and height members to the given values.
       *
       * @param width_ The initial width value.
       * @param height_ The initial height value.
       */
      constexpr Extent(ValueT width_, ValueT height_) noexcept
      : width{width_}, height{height_} {}

    // converting to/from other extents
    template <ExtentLike Other>
      /**
         * @brief Constructs an Extent from an extent-like object.
         *
         * Initializes an Extent object by copying the `width` and `height` values from the provided
         * extent-like object. This constructor enables implicit conversion from any type that exposes
         * these two members.
         *
         * @param extent An object with accessible `width` and `height` members.
         */
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

    /**
     * @brief Constructs a CacheFile instance and initializes the cache file.
     *
     * Attempts to open an existing file or create a new one at the specified path by invoking the 
     * open_or_create method. This ensures that the file is accessible and that its parent directory exists.
     *
     * @param file_path Path to the cache file.
     */
    CacheFile(std::fs::path file_path)
    {
      open_or_create(std::move(file_path));
    }

    /**
     * @brief Opens the specified file and loads its contents into an internal buffer.
     *
     * Attempts to open the file at the given path in binary mode and reads its entire content into an internal byte
     * vector. If the internal buffer is already populated or the file cannot be opened, the function logs an error
     * and returns false.
     *
     * @param file_path The path of the file to open.
     * @return true if the file is successfully opened and its contents are loaded; false otherwise.
     */
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

    /**
     * @brief Attempts to open the specified file, creating it and its parent directory if necessary.
     *
     * This function first tries to open the file using an internal open method. If opening the file fails,
     * it logs an informational message and attempts to create the parent directory for the file, then creates
     * a new file for binary writing. If the file cannot be created, an error is logged and the function returns false.
     *
     * @param file_path The path to the file to be opened or created.
     * @return true if the file was successfully opened or created, false otherwise.
     */
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

    /**
     * @brief Destroys a CacheFile instance.
     *
     * If the internal buffer contains data, this destructor writes its contents to the file at the specified path
     * using binary mode with truncation. If the buffer is empty or the file cannot be opened for writing,
     * no action is taken.
     */
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
