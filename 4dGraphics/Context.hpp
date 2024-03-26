#pragma once

#include "BindlessManager.hpp"
#include "CommandBufferManager.hpp"
#include "DSAllocator.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "VulkanCaches.hpp"
#include "VulkanConstructs.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"
#include "CommandBuffer.hpp"

#include <ankerl/unordered_dense.h>
#include <taskflow/taskflow.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <cassert>
#include <optional>
#include <span>
#include <stack>
#include <vector>

namespace v4dg {
class Context;

class PerQueueFamily {
public:
  enum class Type {
    Graphics,
    AsyncCompute,
    AsyncTransfer,
  };
  struct qt_desc {
    Type type;
    vk::QueueFlags required_flags;
    vk::QueueFlags banned_flags;
  };
  static constexpr std::array<qt_desc, 3> QueueTypes{
      qt_desc{Type::Graphics,
              vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute,
              {}},
      qt_desc{Type::AsyncCompute, vk::QueueFlagBits::eCompute,
              vk::QueueFlagBits::eGraphics},
      qt_desc{Type::AsyncTransfer, vk::QueueFlagBits::eTransfer,
              ~(vk::QueueFlagBits::eTransfer | vk::QueueFlagBits::eSparseBinding)},
  };

  PerQueueFamily() = delete;
  PerQueueFamily(const Queue &queue, const vk::raii::Device &dev);

  auto &queue() const { return *m_queue; }
  const auto &semaphore() const { return m_semaphore; }
  uint64_t semaphoreValue() const { return m_semaphore_value; }

  void setSemaphoreValue(uint64_t value) { m_semaphore_value = value; }

  auto &commandBufferManager(size_t idx) { return m_command_buffer_managers[idx]; }

private:
  const Queue *m_queue;

  // timeline semaphore
  vk::raii::Semaphore m_semaphore;
  uint64_t m_semaphore_value{0};

  per_frame<command_buffer_manager> m_command_buffer_managers;
};

struct PerFrame {
  PerFrame(const vk::raii::Device &device, size_t queues, DSAllocatorWeights weights)
      : m_semaphore_ready_values(queues, 0), m_image_ready(device, {{}, {}}),
        m_render_finished(device, {{}, {}}), m_ds_allocator(device, std::move(weights)) {}
  
  void flush() {
    m_ds_allocator.advance_frame();
    m_destruction_stack.flush();
  }

  std::vector<uint64_t> m_semaphore_ready_values;
  vk::raii::Semaphore m_image_ready;
  vk::raii::Semaphore m_render_finished;

  DSAllocatorPool m_ds_allocator;

  // for the main thread
  DestructionStack m_destruction_stack;
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
    //   so they can be allocated on per queue basis
    command_buffer_manager m_command_buffer_manager;
  };

  PerThread(const vk::raii::Device &device, uint32_t graphics_family)
      : m_per_frame(make_per_frame<PerFrame>(device, graphics_family)) {}

  per_frame<PerFrame> m_per_frame;
};

class Context {
public:
  using QueueType = PerQueueFamily::Type;

  explicit Context(const Device &device, std::optional<DSAllocatorWeights> weights = {});
  ~Context();

  // we will be refering this class as a reference type -> no copying/moving
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;

  auto &instance() const { return m_instance; }
  auto &device() const { return m_device; }

  auto &vkInstance() const { return instance().instance(); }
  auto &vkPhysicalDevice() const { return device().physicalDevice(); }
  auto &vkDevice() const { return device().device(); }

  uint32_t frame_ref() const { return m_frame_idx % max_frames_in_flight; }

  PerFrame &get_frame_ctx() { return m_per_frame[frame_ref()]; }

  PerThread &get_thread_ctx() {
    int id = m_executor.this_worker_id();
    assert(id != -1); // only call from worker thread
    return m_per_thread[id];
  }

  auto &get_thread_frame_ctx() {
    return get_thread_ctx().m_per_frame[frame_ref()];
  }

  DestructionStack &get_destruction_stack() {
    if (std::this_thread::get_id() == m_main_thread_id)
      return get_frame_ctx().m_destruction_stack;
    return get_thread_frame_ctx().m_destruction_stack;
  }

  auto &get_queue(QueueType type) {
    return m_families[static_cast<size_t>(type)];
  }

  void next_frame();

  auto &executor() { return m_executor; }
  auto &pipeline_cache() { return m_pipeline_cache; }

  // wait for all work to finish
  void cleanup();

  CommandBuffer getGraphicsCommandBuffer(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary,
    command_buffer_manager::category cat = command_buffer_manager::category::c0_100) {
    return {
        get_thread_frame_ctx().m_command_buffer_manager.get(level, cat),
        *this
    };
  }

  // only one thread at a time can have a command buffer of a given type
  //  from this function
  CommandBuffer getAnyCommandBuffer(QueueType type,
    vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary,
    command_buffer_manager::category cat = command_buffer_manager::category::c0_100) {
    auto &q = get_queue(type);
    assert(q);
    return {
        q->commandBufferManager(frame_ref()).get(level, cat),
        *this
    };
  }

  BindlessManager &bindlessManager() { return m_bindless_manager; }

private:
  const Instance &m_instance;
  const Device &m_device;

  tf::Executor m_executor;

  std::thread::id m_main_thread_id;

  // single queue per family (for now)
  using PerQueueFamilyArray = std::array<std::optional<PerQueueFamily>,
                                         PerQueueFamily::QueueTypes.size()>;
  PerQueueFamilyArray m_families;

  uint64_t m_frame_idx{0};
  per_frame<PerFrame> m_per_frame;
  std::vector<PerThread> m_per_thread;

  vk::raii::PipelineCache m_pipeline_cache;

  BindlessManager m_bindless_manager;

  static DSAllocatorWeights default_weights(const Device &device);

  PerQueueFamilyArray getFamilies() const;
};
} // namespace v4dg