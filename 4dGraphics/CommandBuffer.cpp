#include "CommandBuffer.hpp"

#include "Context.hpp"

#include <vulkan/vulkan.hpp>

#include <ranges>
#include <algorithm>
#include <vector>
#include <utility>
#include <tuple>
#include <numeric>

using namespace v4dg;
CommandBuffer::CommandBuffer(vulkan_raii_view<vk::raii::CommandBuffer> &&other,
                             Context &context)
    : vulkan_raii_view<vk::raii::CommandBuffer>(std::move(other)),
      m_context(&context), m_device(&context.device()),
      m_ds_allocator(context.get_frame_ctx().m_ds_allocator) {}

vk::SubmitInfo2 SubmitGroup::gather_submitInfo() && {
  for (auto &cb_w : m_command_buffer_wrappers) {
    if (!cb_w)
      throw exception("CommandBuffer declared in SubmitGroup is not bound");

    m_waits.insert(m_waits.end(), cb_w->m_waits.begin(), cb_w->m_waits.end());
    m_command_buffers.push_back(**cb_w);
    m_signals.insert(m_signals.end(), cb_w->m_signals.begin(), cb_w->m_signals.end());
  }

  // we can deduplicate the entries in the wait and signal lists
  auto deduplicate = [](bool is_wait, auto &list) {
    auto proj = [](auto const &a) { return std::tuple{a.semaphore, a.deviceIndex, a.pNext}; };

    auto ok_to_merge = [&](auto const &a, auto const &b) {
      return proj(a) == proj(b) && a.pNext == nullptr && b.pNext == nullptr;
    };

    auto reduce_semaphores = [&](auto &&chunk) {
      vk::SemaphoreSubmitInfo res = chunk.front();

      auto values = chunk | std::views::transform(&vk::SemaphoreSubmitInfo::value);
      auto stageMasks = chunk | std::views::transform(&vk::SemaphoreSubmitInfo::stageMask);

      res.value = is_wait ? std::ranges::max(values) : std::ranges::min(values);
      res.stageMask = std::ranges::fold_left(stageMasks, vk::PipelineStageFlags2{}, std::bit_or{});

      return res;
    };

    std::ranges::sort(list, {}, proj);

    auto res = list
      | std::views::chunk_by(ok_to_merge)
      | std::views::transform(reduce_semaphores);
    
    list = std::vector(res.begin(), res.end());
  };

  deduplicate(true, m_waits);
  deduplicate(false, m_signals);

  return vk::SubmitInfo2{
      {},
      m_waits,
      m_command_buffers,
      m_signals,
  };
}