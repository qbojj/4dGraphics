#pragma once

#include <vulkan/vulkan.hpp>

#include <concepts>

namespace v4dg {
template <class T>
concept vulkan_struct_base = requires(T v) {
  typename T::NativeType;
  { T::allowDuplicate } -> std::convertible_to<bool>;
  { T::structureType } -> std::convertible_to<vk::StructureType>;
  { v.sType } -> std::same_as<vk::StructureType &>;
};

template <class T>
concept vulkan_struct_in = vulkan_struct_base<T> && requires(T v) {
  { v.pNext } -> std::same_as<const void *&>;
};

template <class T>
concept vulkan_struct_out = vulkan_struct_base<T> && requires(T v) {
  { v.pNext } -> std::same_as<void *&>;
};

template <class T>
concept vulkan_struct = vulkan_struct_in<T> || vulkan_struct_out<T>;

template <class T, class U>
concept vulkan_struct_extends =
    vulkan_struct<T> && !!vk::StructExtends<T, U>::value;

template <class T, class U>
concept vulkan_struct_chainable =
    std::same_as<T, U> || vulkan_struct_extends<T, U>;

template <class T>
concept vulkan_handle = requires() {
  { T::objectType } -> std::convertible_to<vk::ObjectType>;
  typename T::CType;
};

template <typename T>
concept vulkan_raii_handle = vulkan_handle<T> && requires(T t) {
  typename T::CppType;
  t.release();
};

template <typename T, typename... Ts>
concept any_of = (std::same_as<T, Ts> || ...);
} // namespace v4dg
