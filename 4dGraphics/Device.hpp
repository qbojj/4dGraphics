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
struct Queue {
  Queue(vk::raii::Queue queue, vk::QueueFlags flags, uint32_t family,
        uint32_t index)
      : queue(std::move(queue)), m_flags(flags), m_family(family),
        m_index(index) {}
  
  vk::raii::Queue queue;
  std::mutex m_mutex;

  vk::QueueFlags m_flags;
  uint32_t m_family, m_index;
};

using extension_storage = vk::ArrayWrapper1D<char,vk::MaxExtensionNameSize>;

class Instance {
public:
  Instance(vk::Optional<const vk::AllocationCallbacks> allocator = nullptr);

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
  Device(Handle<Instance> instance);

  const auto &instance() const { return m_instance; }
  const vk::raii::PhysicalDevice &physicalDevice() const {
    return m_physicalDevice;
  }
  const vk::raii::Device &device() const { return m_device; }

  uint32_t apiVer() const { return m_apiVer; }

  vma::Allocator allocator() const { return *m_allocator; }
  auto extensions() const { return m_extensions; }

  const vk::PhysicalDeviceFeatures2 *features() const { return m_features.get(); }

  template <vulkan_handle T, typename... Args>
  void setDebugName(const T &object, std::format_string<T, Args...> name,
                    Args &&...args) const {
    if constexpr (!is_production)
      return;
    if (!m_instance->debugUtilsEnabled())
      return;
    
    std::string nameStr = std::format(name, std::forward<Args>(args)...);

    m_device.setDebugUtilsObjectNameEXT({
        T::objectType,
        reinterpret_cast<uint64_t>(static_cast<typename T::CType>(object)),
        nameStr.c_str(),
    });
  }

  Queue &getQueue(uint32_t family, uint32_t index) {
    return *m_queues[family][index];
  }

public:
  bool m_accelerationStructure;
  bool m_rayTracingPipeline;
  bool m_rayQuery;

  bool m_meshShader;

private:
  Handle<Instance> m_instance;

  vk::raii::PhysicalDevice m_physicalDevice;

  uint32_t m_apiVer;
  std::vector<extension_storage> m_extensions;
  std::shared_ptr<const vk::PhysicalDeviceFeatures2> m_features;

  vk::raii::Device m_device;
  vma::UniqueAllocator m_allocator;

  std::vector<std::vector<Handle<Queue>>> m_queues;

  bool physicalDeviceSuitable(const vk::raii::PhysicalDevice &) const;

  vk::raii::PhysicalDevice choosePhysicalDevice() const;
  std::vector<extension_storage> chooseExtensions() const;
  std::shared_ptr<const vk::PhysicalDeviceFeatures2> chooseFeatures() const;
  vk::raii::Device initDevice() const;
  vma::UniqueAllocator initAllocator() const;
  std::vector<std::vector<Handle<Queue>>> initQueues() const;
};
} // namespace v4dg
