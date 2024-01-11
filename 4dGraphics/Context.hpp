#pragma once

#include "CommandBufferManager.hpp"
#include "DSAllocator.hpp"
#include "Device.hpp"
#include "VulkanConstructs.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <taskflow/taskflow.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>

namespace v4dg {
class Context;

class PerQueueFamily {
public:
  enum class Type {
    Graphics,
    AsyncCompute,
    AsyncTransfer,
    VideoDecode,
    VideoEncode,
    OpticalFlow,
  };

  PerQueueFamily(Handle<Device> device, uint32_t family);

  auto &device() const { return m_device; }
  auto &queue() const { return m_queue; }
  auto &allocator() const { return m_dsAllocatorPool; }
  
  vk::QueueFlags flags() const { return m_flags; }
  uint32_t timestampValidBits() const { return m_timestampValidBits; }
  vk::Extent3D minImageTransferGranularity() const { return m_minImageTransferGranularity; }

private:
  Handle<Device> m_device;
  DSAllocatorPool m_dsAllocatorPool;

  vk::QueueFlags m_flags;
  uint32_t m_timestampValidBits;
  vk::Extent3D m_minImageTransferGranularity;

  Handle<Queue> m_queue;
  vk::raii::Semaphore m_semaphore;
  uint64_t m_semaphore_value{0};
};

class PerFrame {
public:
private:
  Handle<Device> m_device;
  Handle<Queue> m_queue;
  vk::raii::Fence m_fence;
  vk::raii::Semaphore m_semaphore;
  uint64_t m_semaphore_value{0};
};

// thread context of member of Context executor
class ThreadContext {
public:
  static ThreadContext &get() {
    thread_local ThreadContext ctx;
    return ctx;
  }

  Context &context() const { return *m_context; }
  uint32_t threadId() const { return m_thread_id; }

private:
  friend class Context;
  void init(Context *ctx, uint32_t thread_id) {
    m_context = ctx;
    m_thread_id = thread_id;
  }

  Context *m_context;
  uint32_t m_thread_id;
};

class PerThread { 
public:
private:

};

class Context {
public:
private:
  Handle<Instance> m_instance;
  Handle<Device> m_device;

  tf::Executor m_executor;

  // single queue per family (for now)
  std::vector<PerQueueFamily> m_familes;

  std::vector<PerFrame> m_frames_in_flight;
  
};


} // namespace v4dg