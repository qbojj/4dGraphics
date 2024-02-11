#pragma once

#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <taskflow/taskflow.hpp>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <string>
#include <vector>

namespace v4dg {
class alignas(64) Queue {
public:
  Queue(vk::raii::Queue queue, uint32_t family, uint32_t index,
        vk::QueueFlags flags, uint32_t timestampValidBits,
        vk::Extent3D minImageTransferGranularity)
      : m_queue(std::move(queue)), m_family(family), m_index(index),
        m_flags(flags), m_timestampValidBits(timestampValidBits),
        m_minImageTransferGranularity(minImageTransferGranularity) {}

  const vk::raii::Queue &queue() const { return m_queue; }

  const vk::raii::Queue &lock(std::unique_lock<std::mutex> &lock) {
    lock = std::unique_lock(m_mutex);
    return queue();
  }

  uint32_t family() const { return m_family; }
  uint32_t index() const { return m_index; }
  vk::QueueFlags flags() const { return m_flags; }
  uint32_t timestampValidBits() const { return m_timestampValidBits; }
  vk::Extent3D minImageTransferGranularity() const {
    return m_minImageTransferGranularity;
  }

private:
  vk::raii::Queue m_queue;
  std::mutex m_mutex;

  uint32_t m_family, m_index;

  vk::QueueFlags m_flags;
  uint32_t m_timestampValidBits;
  vk::Extent3D m_minImageTransferGranularity;
};

using extension_storage = vk::ArrayWrapper1D<char, vk::MaxExtensionNameSize>;

class Instance {
public:
  Instance(vk::raii::Context context,
           vk::Optional<const vk::AllocationCallbacks> allocator = nullptr);

  // we will be refering to this class as a reference type -> no copying/moving
  Instance(const Instance &) = delete;
  Instance &operator=(const Instance &) = delete;

  const vk::raii::Context &context() const noexcept { return m_context; }
  const vk::raii::Instance &instance() const noexcept { return m_instance; }

  uint32_t maxApiVer() const noexcept { return m_maxApiVer; }
  uint32_t apiVer() const noexcept { return m_apiVer; }

  auto extensions() const noexcept { return m_extensions; }
  auto layers() const noexcept { return m_layers; }

  bool debugUtilsEnabled() const noexcept { return m_debugUtilsEnabled; }

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

class Device {
public:
  Device(const Instance &instance, vk::SurfaceKHR surface = {});

  // we will be refering to this class as a reference type -> no copying/moving
  Device(const Device &) = delete;
  Device &operator=(const Device &) = delete;

  const auto &instance() const noexcept { return m_instance; }
  const vk::raii::PhysicalDevice &physicalDevice() const noexcept {
    return m_physicalDevice;
  }
  const vk::raii::Device &device() const noexcept { return m_device; }

  uint32_t apiVer() const noexcept { return m_apiVer; }

  vma::Allocator allocator() const noexcept { return *m_allocator; }
  auto extensions() const noexcept { return m_extensions; }

  const vk::PhysicalDeviceFeatures2 *features() const noexcept {
    return m_features.get();
  }

  bool debugNamesAvaiable() const noexcept {
    if constexpr (is_production)
      return false;
    return instance().debugUtilsEnabled();
  }

  template<vulkan_handle T>
  void setDebugNameString(const T &object,
                          std::string_view name) const {
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

public:
  // VK_EXT_mesh_shader
  // meshShader + taskShader
  bool m_meshShader;

  // all 3 extensions are required for ray tracing:
  // VK_KHR_acceleration_structure, VK_KHR_ray_tracing_pipeline,
  // VK_KHR_ray_query
  bool m_rayTracing;

  // VK_EXT_device_fault
  bool m_deviceFault;

private:
  const Instance &m_instance;

  vk::raii::PhysicalDevice m_physicalDevice;

  uint32_t m_apiVer;
  std::vector<extension_storage> m_extensions;
  std::shared_ptr<const vk::PhysicalDeviceFeatures2> m_features;

  vk::raii::Device m_device;
  vma::UniqueAllocator m_allocator;

  std::vector<std::vector<Handle<Queue>>> m_queues;

  bool physicalDeviceSuitable(const vk::raii::PhysicalDevice &,
                              vk::SurfaceKHR) const;

  std::vector<extension_storage>
  enumerateExtensions(const vk::raii::PhysicalDevice &) const;
  vk::raii::PhysicalDevice choosePhysicalDevice(vk::SurfaceKHR) const;
  std::vector<extension_storage> chooseExtensions() const;
  std::shared_ptr<const vk::PhysicalDeviceFeatures2> chooseFeatures() const;
  vk::raii::Device initDevice() const;
  vma::UniqueAllocator initAllocator() const;
  std::vector<std::vector<Handle<Queue>>> initQueues() const;
};
} // namespace v4dg
