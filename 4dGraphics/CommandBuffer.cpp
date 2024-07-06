#include "CommandBuffer.hpp"

#include "CommandBufferManager.hpp"
#include "Context.hpp"

#include <bits/ranges_algo.h>
#include <mutex>
#include <optional>
#include <regex>
#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <numeric>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

using namespace v4dg;

CommandBuffer::CommandBuffer(vulkan_raii_view<vk::raii::CommandBuffer> &&other,
                             Context &context, std::uint32_t family_index,
                             std::unique_lock<std::mutex> lock)
    : vulkan_raii_view<vk::raii::CommandBuffer>(std::move(other)),
      m_context(&context),
      m_ds_allocator(context.get_frame_ctx().m_ds_allocator),
      m_lock(std::move(lock)), m_queue_family_index(family_index) {}

void CommandBuffer::beginDebugLabel(std::string_view name,
                                    glm::vec4 color) noexcept {
  if (!m_context->device().debugNamesAvaiable())
    return;
  (*this)->beginDebugUtilsLabelEXT(
      {name.data(), {color.r, color.g, color.b, color.a}});
}

void CommandBuffer::endDebugLabel() noexcept {
  if (!m_context->device().debugNamesAvaiable())
    return;
  (*this)->endDebugUtilsLabelEXT();
}

void CommandBuffer::insertDebugLabel(std::string_view name,
                                     glm::vec4 color) noexcept {
  if (!m_context->device().debugNamesAvaiable())
    return;
  (*this)->insertDebugUtilsLabelEXT(
      {name.data(), {color.r, color.g, color.b, color.a}});
}

CommandBuffer &SubmitGroup::bind_command_buffer(std::size_t index,
                                                CommandBuffer cb) {
  if (index >= m_command_buffer_wrappers.size())
    throw std::out_of_range("CommandBuffer index out of range");
  if (m_command_buffer_wrappers.at(index).has_value())
    throw exception("CommandBuffer already bound");

  m_command_buffer_wrappers.at(index).emplace(std::move(cb));
  return *m_command_buffer_wrappers[index];
}

SubmitionInfo SubmitGroup::gather_submitInfo() && {
  if (not std::ranges::all_of(m_command_buffer_wrappers,
                              &std::optional<CommandBuffer>::has_value))
    throw exception("CommandBuffer declared in SubmitGroup is not bound");

  auto v = m_command_buffer_wrappers | std::views::as_rvalue |
           std::views::transform([](auto o) { return std::move(o).value(); }) |
           std::ranges::to<std::vector>();

  return SubmitionInfo::gather(v);
}

SubmitionInfo SubmitionInfo::gather(std::span<CommandBuffer> cbs) {
  SubmitionInfo res;

  for (CommandBuffer &cb : cbs) {
    if (*cb == vk::CommandBuffer{})
      assert(false && "CommandBuffer was moved out");

    if (!cb.ended)
      assert(false && "CommandBuffer not was not ended");

    res.waits.insert(res.waits.end(), cb.m_waits.begin(), cb.m_waits.end());
    res.command_buffers.emplace_back(*cb);
    res.signals.insert(res.signals.end(), cb.m_signals.begin(),
                       cb.m_signals.end());
    res.resources.append(std::move(cb.m_resources));
  }

  // we can deduplicate the entries in the wait and signal lists
  auto deduplicate = [](bool is_wait, auto &list) {
    auto proj = [](auto const &a) {
      return std::tuple{a.semaphore, a.deviceIndex, a.pNext};
    };

    auto ok_to_merge = [&](auto const &a, auto const &b) {
      return proj(a) == proj(b) && a.pNext == nullptr && b.pNext == nullptr;
    };

    auto reduce_semaphores = [&](auto &&chunk) {
      vk::SemaphoreSubmitInfo res = chunk.front();

      auto values =
          chunk | std::views::transform(&vk::SemaphoreSubmitInfo::value);
      auto stageMasks =
          chunk | std::views::transform(&vk::SemaphoreSubmitInfo::stageMask);

      res.value = is_wait ? std::ranges::max(values) : std::ranges::min(values);
      res.stageMask = std::ranges::fold_left(
          stageMasks, vk::PipelineStageFlags2{}, std::bit_or{});

      return res;
    };

    std::ranges::sort(list, {}, proj);

    list = list | std::views::chunk_by(ok_to_merge) |
           std::views::transform(reduce_semaphores) |
           std::ranges::to<std::vector>();
  };

  deduplicate(true, res.waits);
  deduplicate(false, res.signals);

  return res;
}

vk::SubmitInfo2 SubmitionInfo::get() const noexcept {
  return vk::SubmitInfo2{
      {},
      waits,
      command_buffers,
      signals,
  };
}