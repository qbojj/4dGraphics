#pragma once

#include "CommandBufferManager.hpp"
#include "DSAllocator.hpp"
#include "Device.hpp"
#include "VulkanConstructs.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"
#include "BindlessManager.hpp"
#include "Swapchain.hpp"

#include <taskflow/taskflow.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <ankerl/unordered_dense.h>

#include <vector>
#include <stack>
#include <cassert>

namespace v4dg {
class Context;

class PerQueueFamily {
public:
  enum class Type {
    Graphics,
    AsyncCompute,
    AsyncTransfer,
  };
  using qt_desc = std::pair<Type, vk::QueueFlags>;
  static constexpr std::array<qt_desc,3> QueueTypes{
    qt_desc{Type::Graphics, vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute},
    qt_desc{Type::AsyncCompute, vk::QueueFlagBits::eCompute},
    qt_desc{Type::AsyncTransfer, vk::QueueFlagBits::eTransfer},
  };

  PerQueueFamily() = delete;
  PerQueueFamily(Handle<Queue> queue, const vk::raii::Device &dev);

  auto &queue() const { return m_queue; }
  const auto &semaphore() const { return m_semaphore; }
  uint64_t semaphoreValue() const { return m_semaphore_value; }

  void setSemaphoreValue(uint64_t value) { m_semaphore_value = value; }

  auto &commandBufferManager() { return m_command_buffer_manager; }

private:
  Handle<Queue> m_queue;

  // timeline semaphore
  vk::raii::Semaphore m_semaphore;
  uint64_t m_semaphore_value{0};

  command_buffer_manager m_command_buffer_manager;
};

struct PerFrame {
  PerFrame(const vk::raii::Device &device, size_t queues)
    : m_semaphore_ready_values(queues, 0)
    , m_image_ready(device, {{}, {}})
    , m_render_finished(device, {{}, {}}) {}
  
  std::vector<uint64_t> m_semaphore_ready_values;
  vk::raii::Semaphore m_image_ready;
  vk::raii::Semaphore m_render_finished;
};

struct PerThread { 
  struct PerFrame {
    PerFrame(const vk::raii::Device &device, uint32_t graphics_family)
      : m_command_buffer_manager(device, graphics_family) {}
    
    void flush() {
      m_destruction_stack.flush();
      m_command_buffer_manager.reset();
    }

    DestructionStack m_destruction_stack;

    // only for graphics queue as async compute/transfer are
    //   are expected to have only small number of command buffers per frame
    //   so they can be 
    command_buffer_manager m_command_buffer_manager;
  };

  PerThread(const vk::raii::Device &device, uint32_t graphics_family)
    : m_per_frame(make_per_frame<PerFrame>(device, graphics_family)) {}

  per_frame<PerFrame> m_per_frame;
};

class Context {
public:
  using QueueType=PerQueueFamily::Type;

  Context(Handle<Instance> instance, Handle<Device> device);

  auto &instance() const { return *m_instance; }
  auto &device() const { return *m_device; }

  auto &vkInstance() const { return instance().instance(); }
  auto &vkPhysicalDevice() const { return device().physicalDevice(); }
  auto &vkDevice() const { return device().device(); }

  uint32_t frame_ref() const { return m_frame_idx % max_frames_in_flight; }

  PerFrame &get_frame_ctx() {
    return m_per_frame[frame_ref()];
  }

  PerThread &get_thread_ctx() {
    int id = m_executor.this_worker_id();
    assert(id != -1); // only call from worker thread
    return m_per_thread[id];
  }

  auto &get_thread_frame_ctx() {
    return get_thread_ctx().m_per_frame[frame_ref()];
  }

  DestructionStack &get_destruction_stack() {
    return get_thread_frame_ctx().m_destruction_stack;
  }

  auto &get_queue(QueueType type) {
    return m_families[static_cast<size_t>(type)];
  }

  void next_frame();

  auto &executor() {
    return m_executor;
  }

private:
  Handle<Instance> m_instance;
  Handle<Device> m_device;

  tf::Executor m_executor;

  // single queue per family (for now)
  using PerQueueFamilyArray = std::array<std::optional<PerQueueFamily>, PerQueueFamily::QueueTypes.size()>;
  PerQueueFamilyArray m_families;

  uint64_t m_frame_idx{0};
  per_frame<PerFrame> m_per_frame;
  std::vector<PerThread> m_per_thread;

  PerQueueFamilyArray getFamilies() const;
};


} // namespace v4dg