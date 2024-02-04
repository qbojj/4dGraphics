#pragma once

#include "Debug.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace v4dg {
#ifdef NDEBUG
constexpr bool is_debug = false;
#else
constexpr bool is_debug = true;
#endif

constexpr bool is_production = false;

constexpr size_t max_frames_in_flight = 3;
template <typename T> using per_frame = std::array<T, max_frames_in_flight>;

namespace detail {
template <typename T, std::size_t N, std::size_t... Is>
[[nodiscard]] constexpr std::array<T, N>
make_array_it_impl(std::index_sequence<Is...>, auto &&fn) {
  return {std::forward<decltype(fn)>(fn)(Is)...};
}
} // namespace detail

template <typename T, std::size_t N>
[[nodiscard]] constexpr std::array<T, N> make_array_it(auto &&fn) {
  return detail::make_array_it_impl<T, N>(std::make_index_sequence<N>{},
                                          std::forward<decltype(fn)>(fn));
}

template <typename T>
[[nodiscard]] constexpr per_frame<T> make_per_frame_it(auto &&fn) {
  return make_array_it<T, max_frames_in_flight>(std::forward<decltype(fn)>(fn));
}

template <typename T>
[[nodiscard]] constexpr per_frame<T> make_per_frame(const auto &...args) {
  return make_per_frame_it<T>(
      [&](std::size_t) { return T{std::forward<decltype(args)>(args)...}; });
}

template <typename T> using Handle = std::shared_ptr<T>;

template <typename T, typename... Args> auto make_handle(Args &&...args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

class exception : public std::runtime_error {
public:
  exception() noexcept = default;

  template <typename... Args>
  explicit exception(Logger::format_string_with_location<Args...> fmt,
                     Args &&...args)
      : std::runtime_error(std::format(fmt.fmt, std::forward<Args>(args)...)) {
    logger.Warning(fmt, std::forward<Args>(args)...);
  }
};

} // namespace v4dg
