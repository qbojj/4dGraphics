#include "CommandBuffer.hpp"

#include "Context.hpp"

namespace v4dg {
CommandBuffer::CommandBuffer(vulkan_raii_view<vk::raii::CommandBuffer> &&other,
                             Context &context)
    : vulkan_raii_view<vk::raii::CommandBuffer>(std::move(other)),
      m_context(context),
      m_device(context.device()),
      m_ds_allocator(context.get_frame_ctx().m_ds_allocator) {}
} // namespace v4dg