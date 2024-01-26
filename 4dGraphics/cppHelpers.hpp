#pragma once

#include <algorithm>
#include <exception>
#include <functional>
#include <string>
#include <string_view>
#include <optional>
#include <fstream>

namespace v4dg::detail {
class destroy_helper {
public:
  destroy_helper() = default;
  destroy_helper(const destroy_helper &) = delete;
  destroy_helper(destroy_helper &&) = default;

  template <typename F>
  destroy_helper(F &&destroy) : destr_(std::forward<F>(destroy)) {}

  ~destroy_helper() {
    if (destr_)
      destr_();
  }

  destroy_helper &operator=(const destroy_helper &) = delete;

  destroy_helper &operator=(destroy_helper &&o) {
    if (this == &o)
      return *this;

    if (destr_)
      destr_();
    destr_ = std::move(o.destr_);

    return *this;
  }

  void clear() { destr_ = nullptr; };

private:
  std::function<void()> destr_;
};

// calls stored function only on exception (must be allocated on the stack)
class exception_guard {
public:
  exception_guard() noexcept
      : count_(std::uncaught_exceptions()), onExcept_() {}

  exception_guard(const exception_guard &) = delete;
  exception_guard(exception_guard &&) = default;

  template <typename F>
  exception_guard(F &&onExcept)
      : count_(std::uncaught_exceptions()),
        onExcept_(std::forward<F>(onExcept)) {}

  ~exception_guard() {
    if (onExcept_ && std::uncaught_exceptions() != count_)
      onExcept_();
  }

  exception_guard &operator=(const exception_guard &) = delete;
  exception_guard &operator=(exception_guard &&) = default;

private:
  int count_;
  std::function<void()> onExcept_;
};

std::optional<std::string> GetStreamString(std::istream &file, bool binary = false);

template<typename P>
std::optional<std::string> GetFileString(P &&pth, bool binary = false) {
  std::ifstream file(std::forward<P>(pth), binary ? std::ios::binary : std::ios::in);
  return GetStreamString(file, binary);
}

template<typename T, typename P>
  requires std::is_trivially_copyable_v<T>
auto GetFileBinary(P &&pth) {
  return GetFileString(std::forward<P>(pth), true).and_then([](auto &&str) -> std::optional<std::vector<T>> {
    if (str.size() % sizeof(T) != 0)
      return std::nullopt;
    std::vector<T> ret(str.size() / sizeof(T));
    std::memcpy(ret.data(), str.data(), str.size());
    return ret;
  });
}


template <typename T, typename V> static inline T AlignUp(T val, V alignment) {
  return (val + static_cast<T>(alignment) - 1) &
         ~(static_cast<T>(alignment) - 1);
}

template <typename T, typename V>
static inline T AlignUpOffset(T val, V alignment) {
  return AlignUp(val, alignment) - val;
}

template <typename T, typename V>
static inline T AlignDown(T val, V alignment) {
  return val & ~(static_cast<T>(alignment) - 1);
}
} // namespace v4dg::detail