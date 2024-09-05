#include "Context.hpp"

#include "CommandBuffer.hpp"
#include "CommandBufferManager.hpp"
#include "DSAllocator.hpp"
#include "Debug.hpp"
#include "Device.hpp"
#include "Queue.hpp"
#include "cppHelpers.hpp"
#include "v4dgCore.hpp"

#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <ios>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <thread>
#include <utility>
#include <vector>

using namespace v4dg;

PerQueueFamily::PerQueueFamily(Context &ctx, const Queue &queue)
    : m_ctx{&ctx}, m_queue{&queue},
      m_semaphore{
          m_ctx->vkDevice(),
          vk::StructureChain{
              vk::SemaphoreCreateInfo{},
              vk::SemaphoreTypeCreateInfo{vk::SemaphoreType::eTimeline, 0},
          }
              .get<>(),
      },
      m_command_buffer_managers{
          make_per_frame<command_buffer_manager>(m_ctx->vkDevice(),
                                                 m_queue->family()),
      } {}

void PerQueueFamily::submit(std::span<SubmitionInfo> infos, vk::Fence fence) {
  ZoneScoped;

  if (infos.empty()) {
    return;
  }

  std::scoped_lock const _{queue_mutex()};

  infos.back().signals.emplace_back(
      *m_semaphore, m_semaphore_value + 1,
      vk::PipelineStageFlagBits2KHR::eBottomOfPipe);

  for (auto &info : infos) {
    // append resources to the resource hold stack
    m_ctx->get_destruction_stack().append(std::move(info.resources));
  }

  m_queue->queue().submit2(infos | std::views::transform(&SubmitionInfo::get) |
                               std::ranges::to<std::vector>(),
                           fence);

  logger.Debug("Submitting {} command groups to queue fam-{}:idx-{}",
               infos.size(), m_queue->family(), m_queue->index());

  for (auto &info : infos) {
    logger.Debug("  command group: {} command buffers",
                 info.command_buffers.size());
    logger.Debug("  command group: {} signal semaphores", info.signals.size());
    for (auto &sig : info.signals) {
      logger.Debug("    signal sem: {} value: {} stages: {}",
                   (void *)(sig.semaphore), sig.value, sig.stageMask);
    }
    logger.Debug("  command group: {} wait semaphores", info.waits.size());
    for (auto &wait : info.waits) {
      logger.Debug("    wait sem: {} value: {} stages: {}",
                   (void *)(wait.semaphore), wait.value, wait.stageMask);
    }
  }

  ++m_semaphore_value;
}

CommandBuffer
PerQueueFamily::getCommandBuffer(vk::CommandBufferLevel level,
                                 command_buffer_manager::category cat) {
  return {
      m_command_buffer_managers[m_ctx->frame_ref()].get(level, cat),
      *m_ctx,
      queue().family(),
      std::unique_lock(m_cbm_mutex),
  };
}

void PerQueueFamily::flush_frame(std::uint32_t frame) {
  m_command_buffer_managers[frame].reset();
}

Context::Context(const Device &dev,
                 const std::optional<DSAllocatorWeights> &weights)
    : m_instance(dev.instance()), m_device(dev),
      m_main_thread_id(std::this_thread::get_id()), m_families(getFamilies()),
      m_per_frame{
          make_per_frame<PerFrame>(vkDevice(), m_families.size(),
                                   weights.value_or(default_weights(dev)))},
      m_pipeline_cache(nullptr), m_bindless_manager(device()) {
  auto &graphics_queue = get_queue(PerQueueFamily::Type::Graphics);
  uint32_t const graphics_family = graphics_queue->queue().family();

  m_per_thread.reserve(m_executor.num_workers());
  for (size_t i{}; i < m_executor.num_workers(); ++i) {
    m_per_thread.emplace_back(vkDevice(), graphics_family);
  }

  auto pipeline_cache_data =
      detail::GetFileString("pipeline_cache.bin").value_or(std::string{});
  m_pipeline_cache = vkDevice().createPipelineCache(
      {{}, pipeline_cache_data.size(), pipeline_cache_data.data()});
}

Context::~Context() {
  try {
    cleanup();
    auto data = m_pipeline_cache.getData();
    std::ofstream("pipeline_cache.bin", std::ios::binary)
        .write(reinterpret_cast<const char *>(data.data()),
               static_cast<std::streamsize>(data.size()));
  } catch (const std::exception &e) {
    logger.Error("Exception in Context destructor: {}", e.what());
  }
}

void Context::cleanup() {
  m_executor.wait_for_all();
  vkDevice().waitIdle();

  for (size_t i{}; i < max_frames_in_flight; ++i) {
    next_frame();
  }
}

auto Context::getFamilies() -> PerQueueFamilyArray {
  static constexpr auto N = PerQueueFamily::QueueTypes.size();
  std::array<std::pair<int, int>, N> families_queues =
      make_array_it<std::pair<int, int>, N>(
          [](auto) { return std::pair{-1, -1}; });

  const auto &all_queues = device().queues();

  auto to_idx = [](QueueType type) { return static_cast<uint32_t>(type); };

  auto has_all_flags = [](vk::QueueFlags flags, vk::QueueFlags required) {
    return (flags & required) == required;
  };
  auto has_any_flags = [](vk::QueueFlags flags, vk::QueueFlags required) {
    return (flags & required) != vk::QueueFlags{};
  };

  auto flag_count = [](vk::QueueFlags flags) {
    return std::popcount(static_cast<std::uint32_t>(flags));
  };

  auto get_at = [&](std::pair<int, int> &pair) -> const Queue & {
    return all_queues[pair.first][pair.second];
  };

  auto get_queue_idx = [&](QueueType type) -> std::pair<int, int> & {
    return families_queues[to_idx(type)];
  };

  auto has_queue = [&](QueueType type) -> bool {
    return get_queue_idx(type) != std::pair{-1, -1};
  };

  auto get_queue = [&](QueueType type) -> const Queue & {
    return get_at(get_queue_idx(type));
  };

  for (const auto &queue_fam : all_queues) {
    if (queue_fam.empty()) {
      continue;
    }

    size_t idx = 0;

    // find queue families with least flags
    for (auto [type, required_flags, banned_flags] :
         PerQueueFamily::QueueTypes) {
      const Queue &queue = queue_fam[idx];

      if (!has_all_flags(queue.flags(), required_flags) ||
          has_any_flags(queue.flags(), banned_flags)) {
        continue;
      }

      if (has_queue(type) &&
          flag_count(get_queue(type).flags()) <= flag_count(queue.flags())) {
        continue;
      }

      get_queue_idx(type) = {queue.family(), queue.index()};

      if (++idx == queue_fam.size()) {
        break;
      }
    }
  }

  if (!has_queue(PerQueueFamily::Type::Graphics)) {
    throw exception("no graphics queue found");
  }

  return make_array_it<N>(
      [&](std::size_t i) -> std::unique_ptr<PerQueueFamily> {
        auto type = PerQueueFamily::QueueTypes[i].type;
        if (!has_queue(type)) {
          return nullptr;
        }

        return std::make_unique<PerQueueFamily>(*this, get_queue(type));
      });
}

void Context::next_frame() {
  ZoneScoped;

  logger.Debug("end of frame {}", m_frame_idx);
  logger.Debug("frame {} summary:", m_frame_idx);
  auto &cur_frame = get_frame_ctx();

  for (auto [i, q] : std::views::enumerate(m_families)) {
    auto &sem_value = cur_frame.m_semaphore_ready_values[i];
    if (q) {
      std::scoped_lock const _{q->queue_mutex()};
      sem_value = q->semaphore_value();
    } else {
      sem_value = 0;
    }

    logger.Debug("  queue {}: sem value {}", i, sem_value);
  }

  // we want to wait for every resource from previous 'next' frame to be not
  // used
  m_frame_idx++;
  auto &next_frame = get_frame_ctx();

  logger.Debug("  waiting for frame {}:",
               static_cast<std::int64_t>(m_frame_idx - max_frames_in_flight));

  std::vector<vk::Semaphore> semaphores;
  std::vector<std::uint64_t> values;
  for (auto [i, q] : std::views::enumerate(m_families)) {
    auto sem_value = next_frame.m_semaphore_ready_values[i];
    if (!q || sem_value == 0) {
      continue;
    }

    semaphores.push_back(*q->semaphore());
    values.push_back(sem_value);
    logger.Debug("    queue {}: sem value {}", i, sem_value);
  }

  if (!semaphores.empty()) {
    ZoneScopedN("wait for semaphores");
    auto result = vkDevice().waitSemaphores(
        {{}, semaphores, values}, std::numeric_limits<uint64_t>::max());
    if (result != vk::Result::eSuccess) {
      throw exception("waitSemaphores failed: {}", result);
    }
  }

  logger.Debug("  waiting ended");
  logger.Debug("moving to frame {}", m_frame_idx);

  {
    ZoneScopedN("clear stacks");

    next_frame.flush();
    for (auto &per_thread : m_per_thread) {
      per_thread.m_per_frame[frame_ref()].flush();
    }
    for (auto &q : m_families) {
      if (q) {
        q->flush_frame(frame_ref());
      }
    }
  }
}

DSAllocatorWeights Context::default_weights(const Device & /*dev*/) {
  // NOLINTBEGIN(*-magic-numbers)
  DSAllocatorWeights weights{
      .m_weights =
          {
              {.type = vk::DescriptorType::eUniformTexelBuffer, .weight = 1.F},
              {.type = vk::DescriptorType::eStorageTexelBuffer, .weight = 1.F},
              {.type = vk::DescriptorType::eUniformBuffer, .weight = 2.F},
              {.type = vk::DescriptorType::eStorageBuffer, .weight = 2.F},
              {.type = vk::DescriptorType::eUniformBufferDynamic,
               .weight = 1.F},
              {.type = vk::DescriptorType::eStorageBufferDynamic,
               .weight = 1.F},
              {.type = vk::DescriptorType::eInputAttachment, .weight = 0.5F},
          },
  };
  // NOLINTEND(*-magic-numbers)

  return weights;
};
