#pragma once

#include "Debug.hpp"
#include "cppHelpers.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <format>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace v4dg {
#ifdef NDEBUG
constexpr bool is_debug = false;
#else
constexpr bool is_debug = true;
#endif

constexpr bool is_production = false;

constexpr size_t max_frames_in_flight = 2;
template <typename T> using per_frame = std::array<T, max_frames_in_flight>;

template <typename T, std::size_t N>
[[nodiscard]] constexpr std::array<T, N>
make_array_it(std::invocable<std::size_t> auto &&fn) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks): false positive
  return
      [&]<std::size_t... Is>(std::index_sequence<Is...>) -> std::array<T, N> {
        return {std::forward<decltype(fn)>(fn)(Is)...};
      }(std::make_index_sequence<N>{});
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

template <std::size_t N>
[[nodiscard]] constexpr decltype(auto)
make_array_it(std::invocable<std::size_t> auto &&fn) {
  return make_array_it<decltype(fn(0z)), N>(std::forward<decltype(fn)>(fn));
}

template <typename T>
[[nodiscard]] constexpr per_frame<T>
make_per_frame_it(std::invocable<std::size_t> auto &&fn) {
  return make_array_it<T, max_frames_in_flight>(std::forward<decltype(fn)>(fn));
}

[[nodiscard]] constexpr decltype(auto)
make_per_frame_it(std::invocable<std::size_t> auto &&fn) {
  return make_per_frame_it<decltype(fn(0z))>(std::forward<decltype(fn)>(fn));
}

template <typename T>
[[nodiscard]] constexpr per_frame<T> make_per_frame(const auto &...args) {
  return make_per_frame_it<T>([&](std::size_t) { return T{args...}; });
}

class exception : public std::runtime_error {
public:
  template <typename... Args>
  explicit exception(Logger::format_string_with_location<Args...> fmt,
                     Args &&...args)
      : std::runtime_error(std::format(fmt.fmt, std::forward<Args>(args)...)) {
    logger.Warning(fmt, std::forward<Args>(args)...);
  }
};

template <typename T> using handle = std::shared_ptr<const T>;
} // namespace v4dg

namespace std {
template <typename T>
  requires requires(const T &t) { v4dg::to_string(t); }
struct formatter<T> : formatter<std::string_view> {
  auto format(const T &t, auto &ctx) const {
    return formatter<std::string_view>::format(v4dg::to_string(t), ctx);
  }
};
} // namespace std
