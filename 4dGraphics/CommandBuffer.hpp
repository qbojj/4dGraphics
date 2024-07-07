#pragma once

#include "Constants.hpp"
#include "DSAllocator.hpp"
#include "Debug.hpp"
#include "Device.hpp"
#include "cppHelpers.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <mutex>
#include <string_view>

namespace v4dg {
class Context;
class SubmitGroup;
class CommandBuffer : public vulkan_raii_view<vk::raii::CommandBuffer> {
public:
  CommandBuffer(vulkan_raii_view<vk::raii::CommandBuffer> &&, Context &,
                std::uint32_t, std::unique_lock<std::mutex>);

  void beginDebugLabel(std::string_view name,
                       glm::vec4 color = constants::vBlack) noexcept;
  void endDebugLabel() noexcept;
  void insertDebugLabel(std::string_view name,
                        glm::vec4 color = constants::vBlack) noexcept;

  [[nodiscard]] auto
  debugLabelScope(std::string_view name,
                  glm::vec4 color = constants::vBlack) noexcept {
    beginDebugLabel(name, color);
    return detail::destroy_helper([this] { endDebugLabel(); });
  }

  [[nodiscard]] Context &context() const noexcept { return *m_context; }
  [[nodiscard]] DSAllocator &ds_allocator() noexcept { return m_ds_allocator; }

  template <typename... Chain>
  void barrier(vk::DependencyFlags flags,
               vk::ArrayProxy<const vk::MemoryBarrier2> memoryBarriers,
               vk::ArrayProxy<const vk::BufferMemoryBarrier2> bufferBarriers,
               vk::ArrayProxy<const vk::ImageMemoryBarrier2> imageBarriers,
               const Chain &...chain_rest) noexcept {
    vk::StructureChain<vk::DependencyInfo, Chain...> chain{
        vk::DependencyInfo{flags, memoryBarriers, bufferBarriers,
                           imageBarriers},
        chain_rest...};
    (*this)->pipelineBarrier2(chain.get());
  }

  void end() noexcept {
    (*this)->end();
    m_lock = {};
    ended = true;
  }

  void add_wait(vk::Semaphore semaphore, uint64_t value,
                vk::PipelineStageFlags2 stage) {
    m_waits.emplace_back(semaphore, value, stage);
  }

  void add_signal(vk::Semaphore semaphore, uint64_t value,
                  vk::PipelineStageFlags2 stage) {
    m_signals.emplace_back(semaphore, value, stage);
  }

  void add_resource(DestructionItem resource) {
    m_resources.push(std::move(resource));
  }

  [[nodiscard]] std::uint32_t queueFamily() const noexcept {
    return m_queue_family_index;
  }

private:
  Context *m_context;
  DSAllocator m_ds_allocator;

  // lock held for cmd buffer manager for non per-thread queues
  std::unique_lock<std::mutex> m_lock;

  std::uint32_t m_queue_family_index;

  std::vector<vk::SemaphoreSubmitInfo> m_waits;
  std::vector<vk::SemaphoreSubmitInfo> m_signals;

  // resources that are used by this command buffer (destruction queue)
  DestructionStack m_resources;

  bool ended{false};

  friend struct SubmitionInfo;
};

// vk::SubmitInfo2 + destruction queue
struct SubmitionInfo {
  static SubmitionInfo gather(std::span<CommandBuffer>);
  static SubmitionInfo gather(CommandBuffer &&cb) { return gather({&cb, 1}); }

  std::vector<vk::SemaphoreSubmitInfo> waits;
  std::vector<vk::CommandBufferSubmitInfo> command_buffers;
  std::vector<vk::SemaphoreSubmitInfo> signals;

  DestructionStack resources;

  [[nodiscard]] vk::SubmitInfo2 get() const noexcept;
};

// collection of command buffers that are in the same submit group
// (vk::SubmitInfo2)
class SubmitGroup {
public:
  explicit SubmitGroup(std::size_t command_buffers)
      : m_command_buffer_wrappers(command_buffers) {}

  explicit SubmitGroup(
      std::vector<std::optional<CommandBuffer>> &&command_buffers)
      : m_command_buffer_wrappers(std::move(command_buffers)) {}

  CommandBuffer &bind_command_buffer(std::size_t index, CommandBuffer cb);

  // gather all the information and move out of the object
  // When this function is called all command buffers must have been bound
  SubmitionInfo gather_submitInfo() &&;

private:
  std::vector<std::optional<CommandBuffer>> m_command_buffer_wrappers;
};
} // namespace v4dg