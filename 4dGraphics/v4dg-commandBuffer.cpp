module;

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

module v4dg;

import :core;

import vulkan_hpp;
import glm;

using namespace v4dg;

CommandBuffer::CommandBuffer(vulkan_raii_view<vk::raii::CommandBuffer> &&other,
                             const Device &device, DSAllocator DSallocator,
                             std::uint32_t family_index,
                             std::unique_lock<std::mutex> lock)
    : vulkan_raii_view<vk::raii::CommandBuffer>(std::move(other)),
      m_device(&device), m_ds_allocator(std::move(DSallocator)),
      m_lock(std::move(lock)), m_queue_family_index(family_index) {}

void CommandBuffer::beginDebugLabel(zstring_view name,
                                    glm::vec4 color) noexcept {
  if (!device().debugNamesAvaiable()) {
    return;
  }
  (*this)->beginDebugUtilsLabelEXT(
      {name.data(), {color.r, color.g, color.b, color.a}});
}

void CommandBuffer::endDebugLabel() noexcept {
  if (!device().debugNamesAvaiable()) {
    return;
  }
  (*this)->endDebugUtilsLabelEXT();
}

void CommandBuffer::insertDebugLabel(zstring_view name,
                                     glm::vec4 color) noexcept {
  if (!device().debugNamesAvaiable()) {
    return;
  }

  (*this)->insertDebugUtilsLabelEXT(
      {name.data(), {color.r, color.g, color.b, color.a}});
}

CommandBuffer &SubmitGroup::bind_command_buffer(std::size_t index,
                                                CommandBuffer cb) {
  if (index >= m_command_buffer_wrappers.size()) {
    throw std::out_of_range("CommandBuffer index out of range");
  }

  auto &opt_cb = m_command_buffer_wrappers[index];
  if (opt_cb.has_value()) {
    throw exception("CommandBuffer already bound");
  }

  return opt_cb.emplace(std::move(cb));
}

SubmitionInfo SubmitGroup::gather_submitInfo() && {
  if (not std::ranges::all_of(m_command_buffer_wrappers,
                              &std::optional<CommandBuffer>::has_value)) {
    throw exception("CommandBuffer declared in SubmitGroup is not bound");
  }

  auto all_cbs =
      m_command_buffer_wrappers | std::views::as_rvalue |
      std::views::transform([](auto o_cb) { return std::move(o_cb).value(); }) |
      std::ranges::to<std::vector>();

  return SubmitionInfo::gather(all_cbs);
}

SubmitionInfo SubmitionInfo::gather(std::span<CommandBuffer> cbs) {
  SubmitionInfo res;

  for (CommandBuffer &cb : cbs) {
    if (*cb == vk::CommandBuffer{}) {
      assert(false && "CommandBuffer was moved out");
    }

    if (!cb.ended) {
      assert(false && "CommandBuffer not was not ended");
    }

    res.waits.insert(res.waits.end(), cb.m_waits.begin(), cb.m_waits.end());
    res.command_buffers.emplace_back(*cb);
    res.signals.insert(res.signals.end(), cb.m_signals.begin(),
                       cb.m_signals.end());
    res.resources.append(std::move(cb.m_resources));
  }

  // we can deduplicate the entries in the wait and signal lists
  auto deduplicate = [](bool is_wait, auto &list) {
    auto proj = [](auto const &sem_info) {
      return std::tuple{sem_info.semaphore, sem_info.deviceIndex,
                        sem_info.pNext};
    };

    auto ok_to_merge = [&](auto const &lhs, auto const &rhs) {
      return proj(lhs) == proj(rhs) && lhs.pNext == nullptr &&
             rhs.pNext == nullptr;
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
