#pragma once

#include "DynamicStructureChain.hpp"
#include "Queue.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <taskflow/taskflow.hpp>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <cstdint>
#include <format>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace v4dg {
using extension_storage = vk::ArrayWrapper1D<char, vk::MaxExtensionNameSize>;
class Instance {
public:
  explicit Instance(
      vk::raii::Context context,
      vk::Optional<const vk::AllocationCallbacks> allocator = nullptr);

  // we will be refering to this class as a reference type -> no copying/moving
  Instance(const Instance &) = delete;
  Instance &operator=(const Instance &) = delete;

  [[nodiscard]] const vk::raii::Context &context() const noexcept {
    return m_context;
  }
  [[nodiscard]] const vk::raii::Instance &instance() const noexcept {
    return m_instance;
  }
  [[nodiscard]] auto &vk() const noexcept { return instance(); }

  [[nodiscard]] uint32_t maxApiVer() const noexcept { return m_maxApiVer; }
  [[nodiscard]] uint32_t apiVer() const noexcept { return m_apiVer; }

  [[nodiscard]] auto extensions() const noexcept { return m_extensions; }
  [[nodiscard]] auto layers() const noexcept { return m_layers; }

  [[nodiscard]] bool debugUtilsEnabled() const noexcept {
    return m_debugUtilsEnabled;
  }

private:
  vk::raii::Context m_context;

  uint32_t m_maxApiVer;
  uint32_t m_apiVer;

  std::vector<extension_storage> m_layers;
  std::vector<extension_storage> m_extensions;

  bool m_debugUtilsEnabled;

  vk::raii::Instance m_instance;
  vk::raii::DebugUtilsMessengerEXT m_debugMessenger;

  std::vector<extension_storage> chooseLayers() const;
  std::vector<extension_storage> chooseExtensions() const;
  vk::raii::Instance initInstance(const vk::AllocationCallbacks *) const;

  static vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo();
};

struct DeviceStats {
  DeviceStats() = default;

  // fill with stats of a physical device
  DeviceStats(const vk::raii::PhysicalDevice &pd,
              std::span<const extension_storage> layers);

  [[nodiscard]] bool has_extension(std::string_view name) const noexcept;

  // add an extension without any features or properties
  void add_extension(std::string_view name);

  // add an extension only with features
  template <vulkan_struct_extends<vk::PhysicalDeviceFeatures2> T>
  void add_extension(std::string_view name) {
    add_extension(name);
    features.add<T>();
  }

  // add an extension only with properties
  template <vulkan_struct_extends<vk::PhysicalDeviceProperties2> T>
  void add_extension(std::string_view name) {
    add_extension(name);
    properties.add<T>();
  }

  // add an extension with features and properties
  template <vulkan_struct_extends<vk::PhysicalDeviceFeatures2> Tf,
            vulkan_struct_extends<vk::PhysicalDeviceProperties2> Tp>
  void add_extension(std::string_view name) {
    add_extension(name);
    features.add<Tf>();
    properties.add<Tp>();
  }

  std::vector<extension_storage> extensions;
  DynamicStructureChain<
      vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features>
      features;
  DynamicStructureChain<vk::PhysicalDeviceProperties2,
                        vk::PhysicalDeviceVulkan11Properties,
                        vk::PhysicalDeviceVulkan12Properties,
                        vk::PhysicalDeviceVulkan13Properties>
      properties;
};

class Device {
public:
  explicit Device(const Instance &instance, vk::SurfaceKHR surface = {});

  // we will be refering to this class as a reference type -> no copying/moving
  Device(const Device &) = delete;
  Device &operator=(const Device &) = delete;

  [[nodiscard]] const auto &instance() const noexcept { return m_instance; }
  [[nodiscard]] const auto &vkInstance() const noexcept {
    return instance().instance();
  }
  [[nodiscard]] const auto &physicalDevice() const noexcept {
    return m_physicalDevice;
  }
  [[nodiscard]] const auto &device() const noexcept { return m_device; }
  [[nodiscard]] const auto &vk() const noexcept { return device(); }
  [[nodiscard]] vma::Allocator allocator() const noexcept {
    return *m_allocator;
  }

  // after device creation features are immutable
  const DeviceStats &stats() const { return m_stats; }

  bool debugNamesAvaiable() const noexcept {
    if constexpr (is_production)
      return false;
    return instance().debugUtilsEnabled();
  }

  template <vulkan_handle T>
  void setDebugNameString(const T &object, std::string_view name) const {
    if constexpr (is_production)
      return;
    if (!instance().debugUtilsEnabled())
      return;

    using CType = T::CType;

    CType handle = {};
    if constexpr (requires(CType t1) { t1 = object; })
      handle = object;
    else if constexpr (requires(CType t1) { t1 = *object; })
      handle = *object;
    else
      static_assert(false, "Cannot get handle from object");

    m_device.setDebugUtilsObjectNameEXT({
        T::objectType,
        reinterpret_cast<uint64_t>(handle),
        name.data(),
    });
  }

  template <typename... Args>
  void setDebugName(const vulkan_handle auto &object,
                    std::format_string<Args...> name, Args &&...args) const {
    if constexpr (is_production)
      return;
    if (!instance().debugUtilsEnabled())
      return;

    setDebugNameString(object, std::format(name, std::forward<Args>(args)...));
  }

  void beginDebugLabel(const vk::raii::CommandBuffer &commandBuffer,
                       std::string_view name,
                       const std::array<float, 4> color = {
                           0.f, 0.f, 0.f, 1.f}) const noexcept {
    if constexpr (is_production)
      return;
    if (!instance().debugUtilsEnabled())
      return;
    commandBuffer.beginDebugUtilsLabelEXT({name.data(), color});
  }

  void
  endDebugLabel(const vk::raii::CommandBuffer &commandBuffer) const noexcept {
    if constexpr (is_production)
      return;
    if (!instance().debugUtilsEnabled())
      return;
    commandBuffer.endDebugUtilsLabelEXT();
  }

  const auto &queues() const noexcept { return m_queues; }

private:
  const Instance &m_instance;
  vk::raii::PhysicalDevice m_physicalDevice;

  DeviceStats m_stats;

  vk::raii::Device m_device;
  vma::UniqueAllocator m_allocator;

  std::vector<std::vector<Queue>> m_queues;

  [[nodiscard]] std::optional<float>
  rankPhysicalDevice(const DeviceStats &, const vk::raii::PhysicalDevice &,
                     vk::SurfaceKHR) const;

  [[nodiscard]] std::pair<DeviceStats, vk::raii::PhysicalDevice>
      choosePhysicalDevice(vk::SurfaceKHR) const;
  [[nodiscard]] DeviceStats chooseFeatures(const DeviceStats &) const;
  [[nodiscard]] vk::raii::Device initDevice() const;
  [[nodiscard]] vma::UniqueAllocator initAllocator() const;
  [[nodiscard]] std::vector<std::vector<Queue>> initQueues() const;
};
} // namespace v4dg
