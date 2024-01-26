#pragma once

#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <taskflow/taskflow.hpp>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <format>
#include <functional>
#include <mutex>
#include <span>
#include <vector>
#include <string>
#include <array>
#include <memory>
#include <new>

namespace v4dg {
class Queue {
public:
  Queue(vk::raii::Queue queue, uint32_t family, uint32_t index,
        vk::QueueFlags flags, uint32_t timestampValidBits,
        vk::Extent3D minImageTransferGranularity)
      : m_queue(std::move(queue)), m_family(family), m_index(index),
        m_flags(flags), m_timestampValidBits(timestampValidBits),
        m_minImageTransferGranularity(minImageTransferGranularity) {}

  const vk::raii::Queue &queue() const {
    return m_queue;
  }

  const vk::raii::Queue &lock(std::unique_lock<std::mutex> &lock) {
    lock = std::unique_lock(m_mutex);
    return queue();
  }

  uint32_t family() const { return m_family; }
  uint32_t index() const { return m_index; }
  vk::QueueFlags flags() const { return m_flags; }
  uint32_t timestampValidBits() const { return m_timestampValidBits; }
  vk::Extent3D minImageTransferGranularity() const { return m_minImageTransferGranularity; }

private:
  vk::raii::Queue m_queue;
  std::mutex m_mutex;

  uint32_t m_family, m_index;
  
  vk::QueueFlags m_flags;
  uint32_t m_timestampValidBits;
  vk::Extent3D m_minImageTransferGranularity;
};

using extension_storage = vk::ArrayWrapper1D<char,vk::MaxExtensionNameSize>;

class Instance {
public:
  Instance(vk::raii::Context context,
    vk::Optional<const vk::AllocationCallbacks> allocator = nullptr);

  const vk::raii::Context &context() const { return m_context; }
  const vk::raii::Instance &instance() const { return m_instance; }

  uint32_t maxApiVer() const { return m_maxApiVer; }
  uint32_t apiVer() const { return m_apiVer; }

  auto extensions() const { return m_extensions; }
  auto layers() const { return m_layers; }

  bool debugUtilsEnabled() const { return m_debugUtilsEnabled; }

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

  const auto &instance() const { return *m_instance; }
  const vk::raii::PhysicalDevice &physicalDevice() const {
    return m_physicalDevice;
  }
  const vk::raii::Device &device() const { return m_device; }

  uint32_t apiVer() const { return m_apiVer; }

  vma::Allocator allocator() const { return *m_allocator; }
  auto extensions() const { return m_extensions; }

  const vk::PhysicalDeviceFeatures2 *features() const { return m_features.get(); }

  template <vulkan_handle T, typename... Args>
  void setDebugName(const T &object, std::format_string<Args...> name,
                    Args &&...args) const {
    if constexpr (!is_production)
      return;
    if (!m_instance->debugUtilsEnabled())
      return;
    
    std::string nameStr = std::format(name, std::forward<Args>(args)...);

    typename T::CType handle = {};
    if constexpr (requires(typename T::CType t1, T t2) { t1 = t2; })
      handle = object;
    else if constexpr (requires(typename T::CType t1, T t2) { t1 = *t2; })
      handle = *object;
    else 
      static_assert(false, "Cannot get handle from object");

    m_device.setDebugUtilsObjectNameEXT({
        T::objectType,
        reinterpret_cast<uint64_t>(handle),
        nameStr.c_str(),
    });
  }

  const auto &queues() const { return m_queues; }

public:
  bool m_accelerationStructure;
  bool m_rayTracingPipeline;
  bool m_rayQuery;

  bool m_meshShader;

private:
  const Instance *m_instance;

  vk::raii::PhysicalDevice m_physicalDevice;

  uint32_t m_apiVer;
  std::vector<extension_storage> m_extensions;
  std::shared_ptr<const vk::PhysicalDeviceFeatures2> m_features;

  vk::raii::Device m_device;
  vma::UniqueAllocator m_allocator;

  std::vector<std::vector<Handle<Queue>>> m_queues;

  bool physicalDeviceSuitable(const vk::raii::PhysicalDevice &, vk::SurfaceKHR) const;

  std::vector<extension_storage> enumerateExtensions(const vk::raii::PhysicalDevice&) const;
  vk::raii::PhysicalDevice choosePhysicalDevice(vk::SurfaceKHR) const;
  std::vector<extension_storage> chooseExtensions() const;
  std::shared_ptr<const vk::PhysicalDeviceFeatures2> chooseFeatures() const;
  vk::raii::Device initDevice() const;
  vma::UniqueAllocator initAllocator() const;
  std::vector<std::vector<Handle<Queue>>> initQueues() const;
};
} // namespace v4dg
