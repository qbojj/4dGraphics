module;

#include <cassert>
#include <concepts>
#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>

export module v4dg:dynamicStructureChain;

import :core;

import vulkan_hpp;

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

export template <vulkan_struct T, vulkan_struct_extends<T>... Tstatic>
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

//////////

detail::DynamicStructureChainBase::DynamicStructureChainBase(
    const DynamicStructureChainBase &o)
    : dynamic_data(o.dynamic_data) {

  fixup(o.dynamic_data.data());
}

auto detail::DynamicStructureChainBase::operator=(
    const DynamicStructureChainBase &o) -> DynamicStructureChainBase & {
  dynamic_data = o.dynamic_data;
  fixup(o.dynamic_data.data());
  return *this;
}

void *detail::DynamicStructureChainBase::expand_storage(std::size_t size) {
  void *old_data = data();
  std::size_t const old_size = dynamic_data.size();
  dynamic_data.insert(dynamic_data.end(), size, std::byte{});
  fixup(old_data);

  void *new_data = dynamic_data.data() + old_size;

  if (old_size > 0) {
    auto *last = reinterpret_cast<vk::BaseOutStructure *>(data());
    while (last->pNext != nullptr) {
      last = last->pNext;
    }
    last->pNext = reinterpret_cast<vk::BaseOutStructure *>(new_data);
  }

  return new_data;
}

void detail::DynamicStructureChainBase::fixup(const void *old_ptr) noexcept {
  void *cur_ptr = data();
  if (cur_ptr == old_ptr) {
    return;
  }

  std::ptrdiff_t const offset = static_cast<std::byte *>(cur_ptr) -
                                static_cast<const std::byte *>(old_ptr);
  auto *el = reinterpret_cast<vk::BaseOutStructure *>(cur_ptr);
  while (el != nullptr) {
    if (el->pNext != nullptr) {
      el->pNext = reinterpret_cast<vk::BaseOutStructure *>(
          reinterpret_cast<std::byte *>(el->pNext) + offset);
    }

    el = el->pNext;
  }
}

void detail::DynamicStructureChainBase::shrink_to_fit() {
  const void *old_data = data();
  dynamic_data.shrink_to_fit();
  fixup(old_data);
}

void detail::swap(DynamicStructureChainBase &a,
                  DynamicStructureChainBase &b) noexcept {
  const void *pa = a.data();
  const void *pb = b.data();
  std::ranges::swap(a.dynamic_data, b.dynamic_data);
  a.fixup(pa);
  b.fixup(pb);
}
} // namespace v4dg
