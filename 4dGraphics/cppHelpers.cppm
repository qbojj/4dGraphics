module;

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <fstream>
#include <functional>
#include <ios>
#include <istream>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

export module v4dg.cppHelpers;

import glm;

namespace v4dg::detail {

export template <std::invocable<> F> class destroy_helper {
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
export template <std::invocable<> F> class exception_guard {
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
    if (std::uncaught_exceptions() > count_) {
      std::invoke(onExcept_);
    }
  }

private:
  int count_;
  F onExcept_;
};

export template <typename... Ts> struct overload_set : Ts... {
  using Ts::operator()...;
};

} // namespace v4dg::detail

namespace v4dg {

export enum class get_file_error : std::uint8_t {
  file_not_found,
  file_size_unaligned,
  io_error
};

export std::expected<std::string, get_file_error>
GetStreamString(std::istream &file) {
  file.seekg(0, std::istream::end);
  auto length = file.tellg();
  file.seekg(0);

  if (!file) {
    return std::unexpected(get_file_error::io_error);
  }

  std::string s;
  s.resize_and_overwrite(length, [&](char *buf, std::size_t len) noexcept {
    file.read(buf, static_cast<std::streamsize>(len));
    return file.gcount();
  });

  if (!file || file.gcount() != length) {
    return std::unexpected(get_file_error::io_error);
  }

  return s;
}

export template <typename T>
  requires std::is_trivially_copyable_v<T>
std::expected<std::vector<T>, get_file_error>
GetStreamBinary(std::istream &file) {
  file.seekg(0, std::istream::end);
  auto length = file.tellg();
  file.seekg(0);

  if (!file) {
    return std::unexpected(get_file_error::io_error);
  }

  if (length % sizeof(T) != 0) {
    return std::unexpected(get_file_error::file_size_unaligned);
  }

  std::vector<T> ret(length / sizeof(T));
  file.read(reinterpret_cast<char *>(ret.data()), length);

  if (!file || file.gcount() != length) {
    return std::unexpected(get_file_error::io_error);
  }

  return ret;
}

export auto GetFileString(const auto &pth, std::ios_base::openmode mode = {})
    -> std::expected<std::string, get_file_error> {
  std::ifstream file(pth, mode);
  if (!file) {
    return std::unexpected(get_file_error::file_not_found);
  }

  return GetStreamString(file);
}

export template <typename T>
  requires std::is_trivially_copyable_v<T>
auto GetFileBinary(const auto &pth,
                   std::ios_base::openmode mode = std::ios_base::binary)
    -> std::expected<std::vector<T>, get_file_error> {
  std::ifstream file(pth, mode);
  if (!file) {
    return std::unexpected(get_file_error::file_not_found);
  }

  return GetStreamBinary<T>(file);
}

export template <std::unsigned_integral T, std::integral V>
constexpr T AlignUp(T val, V alignment) {
  return (val + static_cast<T>(alignment) - 1) &
         ~(static_cast<T>(alignment) - 1);
}

export template <std::unsigned_integral T, std::integral V>
constexpr T AlignUpOffset(T val, V alignment) {
  return AlignUp(val, alignment) - val;
}

export template <std::unsigned_integral T, std::integral V>
constexpr T DivCeil(T val, V alignment) {
  return (val + static_cast<T>(alignment) - 1) / static_cast<T>(alignment);
}

export template <std::unsigned_integral T, std::integral V>
constexpr T AlignDown(T val, V alignment) {
  return val & ~(static_cast<T>(alignment) - 1);
}

export template <typename CharT, typename Traits = std::char_traits<CharT>>
class basic_zstring_view : std::basic_string_view<CharT, Traits> {
  using parent = std::basic_string_view<CharT, Traits>;

public:
  using typename parent::const_iterator;
  using typename parent::const_pointer;
  using typename parent::const_reference;
  using typename parent::const_reverse_iterator;
  using typename parent::difference_type;
  using typename parent::iterator;
  using typename parent::pointer;
  using typename parent::reference;
  using typename parent::reverse_iterator;
  using typename parent::size_type;
  using typename parent::traits_type;
  using typename parent::value_type;

  constexpr basic_zstring_view() noexcept = default;
  constexpr basic_zstring_view(const CharT *s) : parent(s) {}
  constexpr basic_zstring_view(std::nullptr_t) = delete;

  constexpr basic_zstring_view(const std::basic_string<CharT, Traits> &s)
      : parent(s) {}

  using parent::begin;
  using parent::cbegin;
  using parent::cend;
  using parent::crbegin;
  using parent::crend;
  using parent::end;
  using parent::rbegin;
  using parent::rend;

  using parent::operator[];
  using parent::at;
  using parent::back;
  using parent::front;

  [[nodiscard]] constexpr parent::const_pointer data() const noexcept {
    return parent::data(); // NOLINT(bugprone-suspicious-stringview-data-usage)
  }

  using parent::empty;
  using parent::length;
  using parent::max_size;
  using parent::size;

  using parent::remove_prefix;
  // no remove_suffix as it would remove null terminator
  constexpr void swap(basic_zstring_view &s) noexcept {
    parent::swap(static_cast<parent &>(s));
  }

  using parent::copy;

  constexpr basic_zstring_view substr(size_type pos = 0) {
    return basic_zstring_view{parent::substr(pos)};
  }

  constexpr parent substr(size_type pos, size_type count) {
    return parent::substr(pos, count);
  }

  using parent::compare;

  using parent::contains;
  using parent::ends_with;
  using parent::find;
  using parent::find_first_not_of;
  using parent::find_first_of;
  using parent::find_last_not_of;
  using parent::find_last_of;
  using parent::rfind;
  using parent::starts_with;

  using parent::npos;

  constexpr auto
  operator<=>(const basic_zstring_view &) const noexcept = default;

  // can be decayed to string_view
  constexpr operator const parent &() const noexcept { return *this; }

private:
  // unsafe
  explicit basic_zstring_view(parent p) : parent(p) {}
};

export using zstring_view = basic_zstring_view<char>;
export using wzstring_view = basic_zstring_view<wchar_t>;
export using zu8string_view = basic_zstring_view<char8_t>;
export using zu16string_view = basic_zstring_view<char16_t>;
export using zu32string_view = basic_zstring_view<char32_t>;

export namespace detail::views {
template <typename T>
inline constexpr auto static_casted = std::views::transform(
    [](auto &&x) -> T { return static_cast<T>(std::forward<decltype(x)>(x)); });

template <typename T>
inline constexpr auto reinterpret_casted =
    std::views::transform([](auto &&x) -> T {
      return reinterpret_cast<T>(std::forward<decltype(x)>(x));
    });

template <typename T>
inline constexpr auto const_casted = std::views::transform(
    [](auto &&x) -> T { return const_cast<T>(std::forward<decltype(x)>(x)); });

template <typename T>
inline constexpr auto dynamic_casted = std::views::transform([](auto &&x) -> T {
  return dynamic_cast<T>(std::forward<decltype(x)>(x));
});
} // namespace detail::views

namespace detail {
template <typename T>
concept has_x = requires(T t) { t.x; };

template <typename T>
concept has_y = requires(T t) { t.y; };

template <typename T>
concept has_z = requires(T t) { t.z; };

template <typename T>
concept has_width = requires(T t) { t.width; };

template <typename T>
concept has_height = requires(T t) { t.height; };

template <typename T>
concept has_depth = requires(T t) { t.depth; };

template <typename T>
concept vec1_pos_like = has_x<T> && !has_y<T> && !has_z<T>;

template <typename T>
concept vec2_pos_like = has_x<T> && has_y<T> && !has_z<T> &&
                        std::same_as<decltype(T::x), decltype(T::y)>;

template <typename T>
concept vec3_pos_like = has_x<T> && has_y<T> && has_z<T> &&
                        std::same_as<decltype(T::x), decltype(T::y)> &&
                        std::same_as<decltype(T::x), decltype(T::z)>;

template <typename T>
concept vec1_size_like = has_width<T> && !has_height<T> && !has_depth<T>;

template <typename T>
concept vec2_size_like = has_width<T> && has_height<T> && !has_depth<T> &&
                         std::same_as<decltype(T::width), decltype(T::height)>;

template <typename T>
concept vec3_size_like =
    has_width<T> && has_height<T> && has_depth<T> &&
    std::same_as<decltype(T::width), decltype(T::height)> &&
    std::same_as<decltype(T::width), decltype(T::depth)>;

template <typename T>
concept vec1_like = vec1_pos_like<T> || vec1_size_like<T>;

template <typename T>
concept vec2_like = vec2_pos_like<T> || vec2_size_like<T>;

template <typename T>
concept vec3_like = vec3_pos_like<T> || vec3_size_like<T>;

template <typename T>
concept vec_pos_like = vec1_pos_like<T> || vec2_pos_like<T> || vec3_pos_like<T>;

template <typename T>
concept vec_size_like =
    vec1_size_like<T> || vec2_size_like<T> || vec3_size_like<T>;

template <typename T>
concept vec_like = vec1_like<T> || vec2_like<T> || vec3_like<T>;

template <vec_like T> struct vec_like_element;

template <vec_pos_like T> struct vec_like_element<T> {
  using type = decltype(T::x);
};

template <vec_size_like T> struct vec_like_element<T> {
  using type = decltype(T::width);
};

template <vec_like T>
using vec_like_element_t = typename vec_like_element<T>::type;

} // namespace detail

export template <typename ElementT = void>
auto to_glm(detail::vec_like auto t) {
  using namespace detail;
  using T = std::remove_cvref_t<decltype(t)>;
  using RealT = typename std::conditional_t<std::is_void_v<ElementT>,
                                            vec_like_element_t<T>, ElementT>;

  if constexpr (vec1_pos_like<T>) {
    return glm::vec<1, RealT>{t.x};
  } else if constexpr (vec2_pos_like<T>) {
    return glm::vec<2, RealT>{t.x, t.y};
  } else if constexpr (vec3_pos_like<T>) {
    return glm::vec<3, RealT>{t.x, t.y, t.z};
  } else if constexpr (vec1_size_like<T>) {
    return glm::vec<1, RealT>{t.width};
  } else if constexpr (vec2_size_like<T>) {
    return glm::vec<2, RealT>{t.width, t.height};
  } else if constexpr (vec3_size_like<T>) {
    return glm::vec<3, RealT>{t.width, t.height, t.depth};
  } else {
    static_assert(false);
  }
}

export constexpr std::string_view to_string(const get_file_error &e) {
  switch (e) {
    using enum get_file_error;
  case file_not_found:
    return "file not found";
  case file_size_unaligned:
    return "file size unaligned";
  case io_error:
    return "io error";
  default:
    return "unknown error";
  }
}

export bool hasAllFlags(auto flags, auto mask) {
  return (flags & mask) == mask;
}
} // namespace v4dg
