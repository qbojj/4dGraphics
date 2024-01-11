#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <concepts>
#include <format>
#include <memory_resource>

namespace v4dg {
template <class T, class U>
concept vulkan_struct_extends = (bool)vk::StructExtends<T, U>::value;

template <class T>
concept vulkan_handle = requires() {
  { T::objectType } -> std::same_as<vk::ObjectType>;
  typename T::CType;
};

template <typename T, typename U>
  requires vulkan_struct_extends<T, U>
T *getVkStructureFromChain(U *pNextChain) {
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
const T *getVkStructureFromChain(const U *pNextChain) {
  auto *pNext = reinterpret_cast<const T *>(pNextChain);
  while (pNext) {
    if (pNext->sType == T::structureType) {
      return pNext;
    }
    pNext = reinterpret_cast<const T *>(pNext->pNext);
  }
  return nullptr;
}

template <typename T>
concept vulkan_raii_handle =
    requires(T t, const vk::raii::Device &dev, typename T::CppType h) {
      typename T::CType;
      typename T::CppType;
      t.release();
      T(dev, h, {});
    };

template <vulkan_raii_handle T> class vulkan_raii_view : public T {
public:
  explicit vulkan_raii_view(nullptr_t) : T(nullptr) {}
  explicit vulkan_raii_view(const vk::raii::Device &device, T::CppType handle)
      : T(device, handle, {}) {
  } // third arg is allocation callbacks OR parent bot not used if we are a view
  ~vulkan_raii_view() { (void)T::release(); }
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
} // namespace v4dg

namespace std {
template <typename T>
  requires requires(const T &t) {
    { ::vk::to_string(t) } -> std::same_as<std::string>;
  }
struct formatter<T> : formatter<std::string> {
  template <typename FormatContext>
  auto format(const T &t, FormatContext &ctx) const {
    return formatter<std::string>::format(::vk::to_string(t), ctx);
  }
};
} // namespace std