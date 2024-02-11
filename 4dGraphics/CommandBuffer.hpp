#pragma once

#include "DSAllocator.hpp"
#include "Device.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"
#include "cppHelpers.hpp"
#include "Debug.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <string_view>

namespace v4dg {
class Context;
class CommandBuffer : public vulkan_raii_view<vk::raii::CommandBuffer> {
public:
  CommandBuffer(vulkan_raii_view<vk::raii::CommandBuffer> &&, Context &);

  void beginDebugLabel(std::string_view name,
                       std::array<float, 4> color = {0, 0, 0, 1}) noexcept {
    m_device.beginDebugLabel(
        static_cast<const vk::raii::CommandBuffer &>(*this), name, color);
  }
  void endDebugLabel() noexcept {
    m_device.endDebugLabel(static_cast<const vk::raii::CommandBuffer &>(*this));
  }

  auto debugLabelScope(std::string_view name,
                       std::array<float, 4> color = {0, 0, 0, 1}) noexcept {
    beginDebugLabel(name, color);
    return detail::destroy_helper([this] { endDebugLabel(); });
  }

  Context &context() const noexcept { return m_context; }
  DSAllocator &ds_allocator() noexcept { return m_ds_allocator; }

  template<typename... Chain>
  void barrier(vk::DependencyFlags flags,
               vk::ArrayProxy<const vk::MemoryBarrier2> memoryBarriers,
               vk::ArrayProxy<const vk::BufferMemoryBarrier2> bufferBarriers,
               vk::ArrayProxy<const vk::ImageMemoryBarrier2> imageBarriers,
               const Chain&... chain_rest) noexcept {
    vk::StructureChain<vk::DependencyInfo, Chain...> chain{
        vk::DependencyInfo{flags, memoryBarriers, bufferBarriers, imageBarriers},
        chain_rest...};
    (*this)->pipelineBarrier2(chain.get());
  }

private:
  Context &m_context;
  const Device &m_device;
  DSAllocator m_ds_allocator;
};
} // namespace v4dg