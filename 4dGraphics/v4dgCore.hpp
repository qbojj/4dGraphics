#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <stdexcept>

namespace v4dg {
#ifdef NDEBUG
constexpr bool is_debug = false;
#else
constexpr bool is_debug = true;
#endif

#ifdef V4DG_PRODUCTION
constexpr bool is_production = true;
#else
constexpr bool is_production = false;
#endif

constexpr size_t max_frames_in_flight = 3;
template <typename T> using per_frame = std::array<T, max_frames_in_flight>;

template <typename T> using Handle = std::shared_ptr<T>;

template <typename T, typename... Args> auto make_handle(Args &&...args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

class exception : public std::runtime_error {
public:
  template<typename... Args>
  explicit exception(std::format_string<Args...> fmt, Args&&... args) 
    : std::runtime_error(std::format(fmt, std::forward<Args>(args)...)) {}
};
} // namespace v4dg
