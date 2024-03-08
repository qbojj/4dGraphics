#pragma once

#include "DSAllocator.hpp"
#include "Debug.hpp"
#include "Device.hpp"
#include "cppHelpers.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <string_view>

namespace v4dg {
class Context;
class SubmitGroup;
class CommandBuffer : public vulkan_raii_view<vk::raii::CommandBuffer> {
public:
  CommandBuffer(vulkan_raii_view<vk::raii::CommandBuffer> &&, Context &);

  void beginDebugLabel(std::string_view name,
                       std::array<float, 4> color = {0, 0, 0, 1}) noexcept {
    m_device->beginDebugLabel(
        static_cast<const vk::raii::CommandBuffer &>(*this), name, color);
  }
  void endDebugLabel() noexcept {
    m_device->endDebugLabel(static_cast<const vk::raii::CommandBuffer &>(*this));
  }

  auto debugLabelScope(std::string_view name,
                       std::array<float, 4> color = {0, 0, 0, 1}) noexcept {
    beginDebugLabel(name, color);
    return detail::destroy_helper([this] { endDebugLabel(); });
  }

  Context &context() const noexcept { return *m_context; }
  DSAllocator &ds_allocator() noexcept { return m_ds_allocator; }

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

  void add_wait(vk::Semaphore semaphore, uint64_t value,
                vk::PipelineStageFlags2 stage) {
    m_waits.push_back({semaphore, value, stage});
  }

  void add_signal(vk::Semaphore semaphore, uint64_t value,
                  vk::PipelineStageFlags2 stage) {
    m_signals.push_back({semaphore, value, stage});
  }

private:
  Context *m_context;
  const Device *m_device;
  DSAllocator m_ds_allocator;

  std::vector<vk::SemaphoreSubmitInfo> m_waits;
  std::vector<vk::SemaphoreSubmitInfo> m_signals;

  friend class SubmitGroup;
};

// submit info equivalent (from synchronization2)
class SubmitGroup {
public:
  SubmitGroup(size_t command_buffers) {
    m_command_buffer_wrappers.reserve(command_buffers);
  }

  template<typename... Args>
  CommandBuffer &bind_command_buffer(size_t index,
                                     Args &&...args) {
    if (index >= m_command_buffer_wrappers.size())
      throw std::out_of_range("CommandBuffer index out of range");
    if (m_command_buffer_wrappers[index].has_value())
      throw exception("CommandBuffer already bound");
    
    m_command_buffer_wrappers[index].emplace(std::forward<Args>(args)...);
    return *m_command_buffer_wrappers[index];
  }

  vk::SubmitInfo2 gather_submitInfo() &&;

private:
  std::vector<vk::SemaphoreSubmitInfo> m_waits;
  std::vector<vk::CommandBufferSubmitInfo> m_command_buffers;
  std::vector<vk::SemaphoreSubmitInfo> m_signals;

  std::vector<std::optional<CommandBuffer>> m_command_buffer_wrappers;
};
} // namespace v4dg