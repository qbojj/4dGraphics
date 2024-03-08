#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <concepts>
#include <format>
#include <memory_resource>
#include <stack>
#include <functional>

namespace v4dg {
template <class T, class U>
concept vulkan_struct_extends = !!vk::StructExtends<T, U>::value;

template <class T>
concept vulkan_handle = requires() {
  T::objectType;
  typename T::CType;
};

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

[[nodiscard]] vk::BufferUsageFlags2KHR getBufferUsage(const vk::BufferCreateInfo &bci);

template <typename T>
concept vulkan_raii_handle =
    requires(T t) {
      typename T::CType;
      typename T::CppType;
      t.release();
    };

template <vulkan_raii_handle T> class vulkan_raii_view {
public:
  explicit vulkan_raii_view(nullptr_t) noexcept : t(nullptr) {}
  explicit vulkan_raii_view(T t) noexcept : t(std::move(t)) {}
  ~vulkan_raii_view() { (void)t.release(); }

  vulkan_raii_view(const vulkan_raii_view &) = delete;
  vulkan_raii_view(vulkan_raii_view &&o) = default;

  vulkan_raii_view &operator=(vulkan_raii_view o) noexcept {
    std::swap(t, o.t);
    return *this;
  }

  operator const T&() const noexcept { return t; }

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
  bool do_is_equal(const std::pmr::memory_resource &) const noexcept override;
};

using DestructionItem = std::move_only_function<void()>;

class DestructionStack {
public:
  ~DestructionStack() { flush(); }

  DestructionStack() = default;
  DestructionStack(const DestructionStack &) = delete;
  DestructionStack &operator=(const DestructionStack &) = delete;
  DestructionStack(DestructionStack &&) = default;
  DestructionStack &operator=(DestructionStack &&) = default;

  void push(DestructionItem &&func) { m_stack.push(std::move(func)); }

  void flush() {
    while (!m_stack.empty()) {
      m_stack.top()();
      m_stack.pop();
    }
  }
private:
  std::stack<DestructionItem> m_stack;
};
} // namespace v4dg

namespace std {
template <typename T>
  requires requires(const T &t) { ::vk::to_string(t); }
struct formatter<T> : formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const T &t, FormatContext &ctx) const {
    return formatter<std::string_view>::format(::vk::to_string(t), ctx);
  }
};

template <size_t N>
struct formatter<vk::ArrayWrapper1D<char, N>> : formatter<std::string_view> {};
} // namespace std