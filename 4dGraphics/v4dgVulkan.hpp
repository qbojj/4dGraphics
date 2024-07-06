#pragma once

#include "vulkanConcepts.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <cstddef>
#include <format>
#include <functional>
#include <memory_resource>
#include <utility>
#include <vector>

namespace v4dg {

template <typename T, typename U>
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

template <typename T, typename U>
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

[[nodiscard]] vk::BufferUsageFlags2KHR
getBufferUsage(const vk::BufferCreateInfo &bci);

template <vulkan_raii_handle T> class vulkan_raii_view {
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

  const typename T::CppType operator*() const noexcept { return *t; }
  const T *operator->() const noexcept { return &t; }

private:
  T t;
};

class vulkan_memory_resource : public std::pmr::memory_resource {
public:
  vulkan_memory_resource(vk::AllocationCallbacks allocator)
      : m_allocator(allocator) {}

private:
  vk::AllocationCallbacks m_allocator;

  void *do_allocate(std::size_t bytes, std::size_t alignment) override;
  void do_deallocate(void *p, std::size_t, std::size_t) override;
  [[nodiscard]] bool
  do_is_equal(const std::pmr::memory_resource &) const noexcept override;
};

struct DestructionItem {
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
  DestructionItem &operator=(DestructionItem &&) noexcept;
  ~DestructionItem();

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

  void append(DestructionStack &&o);

private:
  std::vector<DestructionItem> m_stack;
};
} // namespace v4dg

namespace std {
template <typename T>
  requires requires(const T &t) { ::vk::to_string(t); }
struct formatter<T> : formatter<std::string_view> {
  auto format(const T &t, auto &ctx) const {
    return formatter<std::string_view>::format(vk::to_string(t), ctx);
  }
};

template <size_t N>
struct formatter<vk::ArrayWrapper1D<char, N>> : formatter<std::string_view> {};
} // namespace std