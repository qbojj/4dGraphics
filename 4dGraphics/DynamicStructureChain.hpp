#pragma once

#include "v4dgVulkan.hpp"
#include "vulkanConcepts.hpp"

#include <vulkan/vulkan.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>

namespace v4dg {
namespace detail {
class DynamicStructureChainBase {
public:
  DynamicStructureChainBase() noexcept = default;
  DynamicStructureChainBase(const DynamicStructureChainBase &);
  DynamicStructureChainBase(DynamicStructureChainBase &&) noexcept = default;
  DynamicStructureChainBase &operator=(const DynamicStructureChainBase &);
  DynamicStructureChainBase &
  operator=(DynamicStructureChainBase &&) noexcept = default;

  [[nodiscard]] void *data() noexcept {
    return dynamic_data.empty() ? nullptr : dynamic_data.data();
  }
  [[nodiscard]] const void *data() const noexcept {
    return dynamic_data.empty() ? nullptr : dynamic_data.data();
  }

  void shrink_to_fit();

  template <vulkan_struct T> T &add_struct() {
    return *new (expand_storage(sizeof(T))) T{};
  }

  friend void swap(DynamicStructureChainBase &a,
                   DynamicStructureChainBase &b) noexcept;

private:
  [[nodiscard]] void *expand_storage(std::size_t);
  void fixup(const void *old_ptr) noexcept;

  std::vector<std::byte> dynamic_data;
};

void swap(DynamicStructureChainBase &a, DynamicStructureChainBase &b) noexcept;

} // namespace detail

template <vulkan_struct T, vulkan_struct_extends<T>... Tstatic>
class DynamicStructureChain {
public:
  DynamicStructureChain() noexcept { link(); }
  DynamicStructureChain(const DynamicStructureChain &o)
      : static_chain(o.static_chain), dynamic(o.dynamic) {

    link();
  }
  DynamicStructureChain(DynamicStructureChain &&o) noexcept
      : static_chain(o.static_chain) {

    dynamic = std::exchange(o.dynamic, {});
    link();
    o.link_dynamic();
  }
  DynamicStructureChain &operator=(const DynamicStructureChain &o) {
    static_chain = o.static_chain;
    dynamic = o.dynamic;
    link();
    return *this;
  }
  DynamicStructureChain &operator=(DynamicStructureChain &&o) noexcept {
    if (this != &o) {
      static_chain = o.static_chain;
      dynamic = std::exchange(o.dynamic, {});
      link();
      o.link_dynamic();
    }
    return *this;
  }

  template <vulkan_struct_chainable<T> Tg = T>
  [[nodiscard]] Tg *get() noexcept {
    if constexpr (any_of<Tg, T, Tstatic...>) {
      return &std::get<Tg>(static_chain);
    } else {
      return getVkStructureFromChain<Tg>(get());
    }
  }

  template <vulkan_struct_chainable<T> Tg = T>
  [[nodiscard]] const Tg *get() const noexcept {
    if constexpr (any_of<Tg, T, Tstatic...>) {
      return &std::get<Tg>(static_chain);
    } else {
      return getVkStructureFromChain<Tg>(get());
    }
  }

  template <vulkan_struct_chainable<T> Tg> Tg &add() {
    assert(!get<Tg>() && "Structure already exists in the chain");
    return dynamic.add_struct<Tg>();
  }

  template <vulkan_struct_chainable<T> Tg>
  [[nodiscard]] Tg &get_or_add() noexcept {
    if (Tg *ptr = get<Tg>()) {
      return *ptr;
    }
    return add<Tg>();
  }

  // assign without disturbing the chain
  template <vulkan_struct_chainable<T> Tg> void assign(Tg o) {
    Tg &t = get_or_add<Tg>();
    auto *pNext = t.pNext;
    t = std::move(o);
    t.pNext = pNext;
  }

  void shrink_to_fit() {
    dynamic.shrink_to_fit();
    link_dynamic();
  }

  friend void swap(DynamicStructureChain &a,
                   DynamicStructureChain &b) noexcept {
    std::ranges::swap(a.static_chain, b.static_chain);
    detail::swap(a.dynamic, b.dynamic);
    a.link();
    b.link();
  }

private:
  static_assert(vk::StructureChainValidation<sizeof...(Tstatic) - 2, T,
                                             Tstatic...>::valid);

  template <std::size_t I> void link_static_single_at() noexcept {
    std::get<I>(static_chain).pNext =
        static_cast<void *>(&std::get<I + 1>(static_chain));
  }

  // create/fix the structure schain
  void link() noexcept {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (link_static_single_at<Is>(), ...);
    }(std::make_index_sequence<sizeof...(Tstatic)>{});

    link_dynamic();
  }

  auto &static_tail() noexcept {
    return std::get<sizeof...(Tstatic)>(static_chain);
  }

  void link_dynamic() noexcept {
    // link the last static element to the first dynamic element
    static_tail().pNext = dynamic.data();
  }

  std::tuple<T, Tstatic...> static_chain;
  detail::DynamicStructureChainBase dynamic;
};
} // namespace v4dg