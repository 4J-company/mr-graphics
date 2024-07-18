#pragma once

namespace mr {
  template <typename... Ts>
    struct Overloads : Ts... {
      using Ts::operator()...;
  };

  struct Extent {
    size_t width {};
    size_t height {};
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
