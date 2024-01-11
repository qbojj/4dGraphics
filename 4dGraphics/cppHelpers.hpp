#pragma once

#include <algorithm>
#include <exception>
#include <functional>
#include <string>
#include <string_view>

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

std::string GetFileString(std::string_view pth, bool binary);

template <typename T, typename V> static inline T AlignUp(T val, V alignment) {
  return (val + static_cast<T>(alignment) - 1) &
         ~(static_cast<T>(alignment) - 1);
}

template <typename T, typename V>
static inline T AlignDown(T val, V alignment) {
  return val & ~(static_cast<T>(alignment) - 1);
}
} // namespace v4dg::detail