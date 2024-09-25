module;

#include <array>
#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

export module v4dg:core;

import v4dg.logger;
import v4dg.cppHelpers;

import vulkan_hpp;

namespace v4dg {
#ifdef NDEBUG
export constexpr bool is_debug = false;
#else
export constexpr bool is_debug = true;
#endif

export constexpr bool is_production = true;

export constexpr size_t max_frames_in_flight = 2;
export template <typename T>
using per_frame = std::array<T, max_frames_in_flight>;

export template <typename T, std::size_t N>
[[nodiscard]] constexpr std::array<T, N>
make_array_it(std::invocable<std::size_t> auto &&fn) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks): false positive
  return
      [&]<std::size_t... Is>(std::index_sequence<Is...>) -> std::array<T, N> {
        return {std::forward<decltype(fn)>(fn)(Is)...};
      }(std::make_index_sequence<N>{});
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

export template <std::size_t N>
[[nodiscard]] constexpr decltype(auto)
make_array_it(std::invocable<std::size_t> auto &&fn) {
  return make_array_it<decltype(fn(0z)), N>(std::forward<decltype(fn)>(fn));
}

export template <typename T>
[[nodiscard]] constexpr per_frame<T>
make_per_frame_it(std::invocable<std::size_t> auto &&fn) {
  return make_array_it<T, max_frames_in_flight>(std::forward<decltype(fn)>(fn));
}

export [[nodiscard]] constexpr decltype(auto)
make_per_frame_it(std::invocable<std::size_t> auto &&fn) {
  return make_per_frame_it<decltype(fn(0z))>(std::forward<decltype(fn)>(fn));
}

export template <typename T>
[[nodiscard]] constexpr per_frame<T> make_per_frame(const auto &...args) {
  return make_per_frame_it<T>([&](std::size_t) { return T{args...}; });
}

export class exception : public std::runtime_error {
public:
  template <typename... Args>
  explicit exception(Logger::format_string_with_location<Args...> fmt,
                     Args &&...args)
      : std::runtime_error(
            std::format(fmt.get(), std::forward<Args>(args)...)) {
    logger.Warning(fmt, std::forward<Args>(args)...);
  }
};

export template <typename T> using handle = std::shared_ptr<const T>;

export template <class T>
concept vulkan_struct_base = requires(T v) {
  typename T::NativeType;
  { T::allowDuplicate } -> std::convertible_to<bool>;
  { T::structureType } -> std::convertible_to<vk::StructureType>;
  { v.sType } -> std::same_as<vk::StructureType &>;
};

export template <class T>
concept vulkan_struct_in = vulkan_struct_base<T> && requires(T v) {
  { v.pNext } -> std::same_as<const void *&>;
};

export template <class T>
concept vulkan_struct_out = vulkan_struct_base<T> && requires(T v) {
  { v.pNext } -> std::same_as<void *&>;
};

export template <class T>
concept vulkan_struct = vulkan_struct_in<T> || vulkan_struct_out<T>;

export template <class T, class U>
concept vulkan_struct_extends =
    vulkan_struct<T> && !!vk::StructExtends<T, U>::value;

export template <class T, class U>
concept vulkan_struct_chainable =
    std::same_as<T, U> || vulkan_struct_extends<T, U>;

export template <class T>
concept vulkan_handle = requires() {
  { T::objectType } -> std::convertible_to<vk::ObjectType>;
  typename T::CType;
};

export template <typename T>
concept vulkan_raii_handle = vulkan_handle<T> && requires(T t) {
  typename T::CppType;
  t.release();
};

export template <typename T, typename... Ts>
concept any_of = (std::same_as<T, Ts> || ...);

export template <typename T, typename U>
  requires vulkan_struct_extends<T, U>
[[nodiscard]] T *getVkStructureFromChain(U *pNextChain) {
  auto *pNext = reinterpret_cast<T *>(pNextChain);
  while (pNext) {
    if (pNext->sType == T::structureType) {
      return pNext;
    }
    pNext = reinterpret_cast<T *>(pNext->pNext);
  }
  return nullptr;
}

export template <typename T, typename U>
  requires vulkan_struct_extends<T, U>
[[nodiscard]] const T *getVkStructureFromChain(const U *pNextChain) {
  auto *pNext = reinterpret_cast<const T *>(pNextChain);
  while (pNext) {
    if (pNext->sType == T::structureType) {
      return pNext;
    }
    pNext = reinterpret_cast<const T *>(pNext->pNext);
  }
  return nullptr;
}

export [[nodiscard]] vk::BufferUsageFlags2KHR
getBufferUsage(const vk::BufferCreateInfo &bci) {
  if (const auto *usage =
          getVkStructureFromChain<vk::BufferUsageFlags2CreateInfoKHR>(&bci)) {
    return usage->usage;
  }
  return static_cast<vk::BufferUsageFlags2KHR>(uint32_t{bci.usage});
}

class DestructionItem {
public:
  DestructionItem(std::invocable<> auto &&func)
      : item(std::in_place_type_t<fun_t>{},
             std::forward<decltype(func)>(func)) {}

  template <typename T = void>
  DestructionItem(std::shared_ptr<T> ptr)
      : item(std::in_place_type_t<ptr_t>{},
             std::static_pointer_cast<const void>(std::move(ptr))) {}

  DestructionItem() = default;
  DestructionItem(const DestructionItem &) = delete;
  DestructionItem &operator=(const DestructionItem &) = delete;
  DestructionItem(DestructionItem &&) noexcept = default;
  DestructionItem &operator=(DestructionItem &&o) noexcept {
    DestructionItem it{std::move(o)};
    std::swap(item, it.item);
    return *this;
  }

  // NOLINTNEXTLINE(bugprone-exception-escape): false positive
  ~DestructionItem() {
    if (item.valueless_by_exception()) {
      return;
    }

    std::visit(detail::overload_set{
                   [](std::monostate) noexcept {},
                   [](fun_t &f) noexcept {
                     if (f) {
                       f();
                     }
                   },
                   [](ptr_t &p) noexcept { p.reset(); },
               },
               item);
  }

private:
  using fun_t = std::move_only_function<void() noexcept>;
  using ptr_t = std::shared_ptr<const void>;
  std::variant<std::monostate, fun_t, ptr_t> item;
};

class DestructionStack {
public:
  void push(auto &&...args) {
    m_stack.emplace_back(std::forward<decltype(args)>(args)...);
  }
  void flush() noexcept { m_stack.clear(); }

  void append(DestructionStack &&o) {
    auto &&src = std::move(o).m_stack;
    m_stack.insert(m_stack.end(), std::make_move_iterator(src.begin()),
                   std::make_move_iterator(src.end()));
    src.clear();
  }

private:
  std::vector<DestructionItem> m_stack;
};
} // namespace v4dg

template <typename T>
concept vulkan_formattable = requires(const T &t) { ::vk::to_string(t); };

template <v4dg::vulkan_raii_handle T> class vulkan_raii_view {
public:
  explicit vulkan_raii_view(std::nullptr_t) noexcept : t(nullptr) {}
  explicit vulkan_raii_view(T t) noexcept : t(std::move(t)) {}
  ~vulkan_raii_view() { (void)t.release(); }

  vulkan_raii_view(const vulkan_raii_view &) = delete;
  vulkan_raii_view(vulkan_raii_view &&o) = default;
  vulkan_raii_view &operator=(const vulkan_raii_view &) = delete;
  vulkan_raii_view &operator=(vulkan_raii_view &&o) noexcept {
    if (this != &o) {
      (void)t.release();
      t = std::move(o.t);
    }
    return *this;
  }

  operator const T &() const noexcept { return t; }

  typename T::CppType operator*() const noexcept { return *t; }
  const T *operator->() const noexcept { return &t; }

private:
  T t;
};

namespace std {
export template <vulkan_formattable T>
struct formatter<T> : formatter<std::string_view> {
  auto format(const T &t, auto &ctx) const {
    return formatter<std::string_view>::format(::vk::to_string(t), ctx);
  }
};

export template <size_t N>
struct formatter<vk::ArrayWrapper1D<char, N>> : formatter<std::string_view> {};

export template <typename T>
  requires requires(const T &t) { v4dg::to_string(t); }
struct formatter<T> : formatter<std::string_view> {
  auto format(const T &t, auto &ctx) const {
    return formatter<std::string_view>::format(v4dg::to_string(t), ctx);
  }
};
} // namespace std
