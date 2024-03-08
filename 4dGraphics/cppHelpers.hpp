#pragma once

#include <algorithm>
#include <concepts>
#include <exception>
#include <expected>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace v4dg::detail {

template <std::invocable<> F> class destroy_helper {
public:
  destroy_helper() = delete;
  destroy_helper(const destroy_helper &) = delete;
  destroy_helper(destroy_helper &&) = delete;
  destroy_helper &operator=(const destroy_helper &) = delete;
  destroy_helper &operator=(destroy_helper &&) = delete;

  explicit destroy_helper(F &&destroy) : destr_(std::move(destroy)) {}
  explicit destroy_helper(const F &destroy) : destr_(destroy) {}

  ~destroy_helper() { std::invoke(destr_); }

private:
  F destr_;
};

// calls stored function only on exception (must be allocated on the stack)
template <std::invocable<> F> class exception_guard {
public:
  exception_guard() = delete;
  exception_guard(const exception_guard &) = delete;
  exception_guard(exception_guard &&) = delete;
  exception_guard &operator=(const exception_guard &) = delete;
  exception_guard &operator=(exception_guard &&) = delete;

  explicit exception_guard(F &&onExcept)
      : count_(std::uncaught_exceptions()), onExcept_(std::move(onExcept)) {}

  explicit exception_guard(const F &onExcept)
      : count_(std::uncaught_exceptions()), onExcept_(onExcept) {}

  ~exception_guard() {
    if (std::uncaught_exceptions() > count_)
      std::invoke(onExcept_);
  }

private:
  int count_;
  F onExcept_;
};

enum class get_file_error { file_not_found, file_size_unaligned, io_error };

// takes case of potential BOM and reads rest of the file into a string
inline std::expected<std::string, get_file_error>
GetStreamString(std::istream &file) {
  if (!file)
    return std::unexpected(get_file_error::file_not_found);

  file.seekg(0, file.end);
  auto length = file.tellg();
  file.seekg(0);

  if (length >= 3) {
    // try removing UTF-8 BOM (if present)
    static constexpr unsigned char BOM[] = {0xEF, 0xBB, 0xBF};
    char buf[3];
    file.read(buf, 3);

    if (memcmp(buf, BOM, 3) == 0) {
      // BOM detected
      length -= 3;
    } else {
      // no BOM detected -> revert changes
      file.seekg(0);
    }
  }

  auto exceptions = file.exceptions();
  file.exceptions({}); // disable exceptions

  std::string s;
  s.resize_and_overwrite(length, [&](char *buf, std::size_t len) {
    file.read(buf, len);
    return file.gcount();
  });

  file.exceptions(exceptions); // restore exceptions

  if (file.fail())
    return std::unexpected(get_file_error::io_error);

  return s;
}

template <typename T>
  requires std::is_trivially_copyable_v<T>
inline std::expected<std::vector<T>, get_file_error>
GetStreamBinary(std::istream &file) {
  if (!file)
    return std::unexpected(get_file_error::file_not_found);

  file.seekg(0, file.end);
  auto length = file.tellg();
  file.seekg(0);

  if (length % sizeof(T) != 0)
    return std::unexpected(get_file_error::file_size_unaligned);

  std::vector<T> ret(length / sizeof(T));
  file.read(reinterpret_cast<char *>(ret.data()), length);
  if (file.fail())
    return std::unexpected(get_file_error::io_error);

  return ret;
}

template <typename P>
inline auto GetFileString(P &&pth, std::ios_base::openmode mode = {}) {
  std::ifstream file(std::forward<P>(pth), mode);
  return GetStreamString(file);
}

template <typename T, typename P>
  requires std::is_trivially_copyable_v<T>
inline auto
GetFileBinary(P &&pth, std::ios_base::openmode mode = std::ios_base::binary) {
  std::ifstream file(std::forward<P>(pth), mode);
  return GetStreamBinary<T>(file);
}

template <std::unsigned_integral T, std::integral V>
constexpr inline T AlignUp(T val, V alignment) {
  return (val + static_cast<T>(alignment) - 1) &
         ~(static_cast<T>(alignment) - 1);
}

template <std::unsigned_integral T, std::integral V>
constexpr inline T AlignUpOffset(T val, V alignment) {
  return AlignUp(val, alignment) - val;
}

template <std::unsigned_integral T, std::integral V>
constexpr inline T DivCeil(T val, V alignment) {
  return (val + static_cast<T>(alignment) - 1) / static_cast<T>(alignment);
}

template <std::unsigned_integral T, std::integral V>
constexpr inline T AlignDown(T val, V alignment) {
  return val & ~(static_cast<T>(alignment) - 1);
}

template <typename... Ts> struct overload_set : Ts... {
  using Ts::operator()...;
};
} // namespace v4dg::detail

namespace v4dg {
constexpr inline std::string_view to_string(const detail::get_file_error &e) {
  switch (e) {
  case detail::get_file_error::file_not_found:
    return "file not found";
  case detail::get_file_error::file_size_unaligned:
    return "file size unaligned";
  case detail::get_file_error::io_error:
    return "io error";
  default:
    return "unknown error";
  }
}
} // namespace v4dg