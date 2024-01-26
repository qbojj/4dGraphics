#include "CommandBufferManager.hpp"

#include <tracy/Tracy.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace v4dg {
command_buffer_manager::command_buffer_manager(const vk::raii::Device &dev,
                                               uint32_t family,
                                               vk::CommandPoolCreateFlags flags)
    : m_device(&dev), m_pool(*m_device, {flags, family}) {}

void command_buffer_manager::reset(vk::CommandPoolResetFlags flags) {
  m_pool.reset(flags);

  for (cache_bucket &bucket : m_primary)
    bucket.used = 0;

  for (cache_bucket &bucket : m_secondary)
    bucket.used = 0;
}

vulkan_raii_view<vk::raii::CommandBuffer>
command_buffer_manager::get(vk::CommandBufferLevel level,
                                          category cat) {
  uint32_t cat_idx = static_cast<uint32_t>(cat);
  cache_bucket &pool =
      (level == vk::CommandBufferLevel::ePrimary ? m_primary
                                                 : m_secondary)[cat_idx];

  size_t cmdBuffCnt = pool.buffers.size();
  size_t used = pool.used;
  if (used == cmdBuffCnt) {
    ZoneScopedN("CommandBufferManager::get::allocate");
    pool.buffers.reserve(cmdBuffCnt + block_count);

    auto cbs = m_pool.getDevice().allocateCommandBuffers(
        {*m_pool, level, block_count}, *m_pool.getDispatcher());

    pool.buffers.insert(pool.buffers.end(), cbs.begin(), cbs.end());
  }

  return vulkan_raii_view<vk::raii::CommandBuffer>{
      {*m_device, pool.buffers[pool.used++], *m_pool}};
}
} // namespace v4dg