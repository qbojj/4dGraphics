#include "Context.hpp"

#include "CommandBufferManager.hpp"
#include "Debug.hpp"
#include "cppHelpers.hpp"
#include "v4dgCore.hpp"

#include <taskflow/taskflow.hpp>
#include <tracy/Tracy.hpp>

#include <bit>
#include <exception>
#include <ranges>
#include <utility>
#include <vector>

namespace v4dg {
namespace {
class WorkerInterface : public tf::WorkerInterface {
public:
  void scheduler_prologue(tf::Worker &worker) override {
    // set thread name
    std::string name = "v4dg worker " + std::to_string(worker.id());
    tracy::SetThreadName(name.c_str());
  }

  void scheduler_epilogue(tf::Worker &, std::exception_ptr) override {
    // do nothing
  }
};
} // namespace
PerQueueFamily::PerQueueFamily(const Queue &queue, const vk::raii::Device &dev)
    : m_queue(&queue), m_semaphore(nullptr),
      m_command_buffer_managers(
          make_per_frame<command_buffer_manager>(dev, m_queue->family())) {
  vk::StructureChain<vk::SemaphoreCreateInfo, vk::SemaphoreTypeCreateInfo> sci{
      {}, {vk::SemaphoreType::eTimeline, 0}};

  m_semaphore = {dev, sci.get<>()};
}

Context::Context(const Device &dev, std::optional<DSAllocatorWeights> weights)
    : m_instance(dev.instance()), m_device(dev),
      m_executor(std::thread::hardware_concurrency(),
                 std::make_shared<WorkerInterface>()),
      m_main_thread_id(std::this_thread::get_id()), m_families(getFamilies()),
      m_per_frame{
          make_per_frame<PerFrame>(vkDevice(), m_families.size(),
                                   weights.value_or(default_weights(dev)))},
      m_pipeline_cache(nullptr), m_bindless_manager(device()) {
  auto &graphics_queue = get_queue(PerQueueFamily::Type::Graphics);
  uint32_t graphics_family = graphics_queue->queue().family();

  m_per_thread.reserve(m_executor.num_workers());
  for (size_t i{}; i < m_executor.num_workers(); ++i)
    m_per_thread.emplace_back(vkDevice(), graphics_family);

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
        .write(reinterpret_cast<const char *>(data.data()), data.size());
  } catch (const std::exception &e) {
    logger.Error("Exception in Context destructor: {}", e.what());
  }
}

void Context::cleanup() {
  m_executor.wait_for_all();
  vkDevice().waitIdle();

  for (size_t i{}; i < max_frames_in_flight; ++i)
    next_frame();
}

auto Context::getFamilies() const -> PerQueueFamilyArray {
  static constexpr auto N = PerQueueFamily::QueueTypes.size();
  std::array<std::pair<int, int>, N> families_queues =
      make_array_it<std::pair<int, int>, N>([](auto) {
        return std::pair{-1, -1};
      });

  const auto &all_queues = device().queues();

  auto to_idx = [](QueueType type) { return static_cast<uint32_t>(type); };

  auto has_all_flags = [](vk::QueueFlags flags, vk::QueueFlags required) {
    return (flags & required) == required;
  };
  auto has_any_flags = [](vk::QueueFlags flags, vk::QueueFlags required) {
    return (flags & required) != vk::QueueFlags{};
  };

  auto flag_count = [](vk::QueueFlags flags) {
    return std::popcount(static_cast<uint32_t>(flags));
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
    if (queue_fam.empty())
      continue;

    size_t idx = 0;

    // find queue families with least flags
    for (auto [type, required_flags, banned_flags] :
         PerQueueFamily::QueueTypes) {
      const Queue &queue = queue_fam[idx];

      if (!has_all_flags(queue.flags(), required_flags) ||
          has_any_flags(queue.flags(), banned_flags))
        continue;

      if (has_queue(type) &&
          flag_count(get_queue(type).flags()) <= flag_count(queue.flags()))
        continue;

      get_queue_idx(type) = {queue.family(), queue.index()};

      if (++idx == queue_fam.size())
        break;
    }
  }

  if (!has_queue(PerQueueFamily::Type::Graphics))
    throw exception("no graphics queue found");

  return make_array_it<std::optional<PerQueueFamily>, N>(
      [&](auto i) -> std::optional<PerQueueFamily> {
        auto type = PerQueueFamily::QueueTypes[i].type;
        if (!has_queue(type))
          return std::nullopt;

        return PerQueueFamily{get_queue(type), vkDevice()};
      });
}

void Context::next_frame() {
  ZoneScoped;

  logger.Log("end of frame {}", m_frame_idx);

  logger.Debug("frame {} summary:", m_frame_idx);
  auto &cur_frame = get_frame_ctx();

  for (auto [i, q] : std::views::enumerate(m_families)) {
    cur_frame.m_semaphore_ready_values[i] = q ? q->semaphoreValue() : 0;
    if (q)
      logger.Debug("  queue {}: sem value {}", i,
                   cur_frame.m_semaphore_ready_values[i]);
  }

  // we want to wait for every resource from previous 'next' frame to be not
  // used
  m_frame_idx++;
  auto &next_frame = get_frame_ctx();

  logger.Debug("  waiting for frame {}:",
               static_cast<int64_t>(m_frame_idx - max_frames_in_flight));

  std::vector<vk::Semaphore> semaphores;
  std::vector<uint64_t> values;
  for (auto [i, q] : std::views::enumerate(m_families)) {
    if (q) {
      semaphores.push_back(*q->semaphore());
      values.push_back(next_frame.m_semaphore_ready_values[i]);
      logger.Debug("    queue {}: sem value {}", i, values.back());
    }
  }

  {
    ZoneScopedN("wait for semaphores");
    auto result = vkDevice().waitSemaphores(
        {{}, semaphores, values}, std::numeric_limits<uint64_t>::max());
    if (result != vk::Result::eSuccess)
      throw exception("waitSemaphores failed: {}", result);
  }

  logger.Debug("  waiting ended");

  logger.Log("moving to frame {}", m_frame_idx);

  {
    ZoneScopedN("clear stacks");

    next_frame.flush();
    for (auto &per_thread : m_per_thread)
      per_thread.m_per_frame[frame_ref()].flush();
    for (auto &q : m_families)
      if (q)
        q->commandBufferManager(frame_ref()).reset();
  }
}

DSAllocatorWeights Context::default_weights(const Device &dev) {
  DSAllocatorWeights weights{
      .m_weights =
          {
              {vk::DescriptorType::eSampler, 0.5f},
              {vk::DescriptorType::eCombinedImageSampler, 4.f},
              {vk::DescriptorType::eSampledImage, 4.f},
              {vk::DescriptorType::eStorageImage, 1.f},
              {vk::DescriptorType::eUniformTexelBuffer, 1.f},
              {vk::DescriptorType::eStorageTexelBuffer, 1.f},
              {vk::DescriptorType::eUniformBuffer, 2.f},
              {vk::DescriptorType::eStorageBuffer, 2.f},
              {vk::DescriptorType::eUniformBufferDynamic, 1.f},
              {vk::DescriptorType::eStorageBufferDynamic, 1.f},
              {vk::DescriptorType::eInputAttachment, 0.5f},
          },
  };

  // add acceleration structure if applicable
  if (dev.m_rayTracing)
    weights.m_weights.push_back(
        {vk::DescriptorType::eAccelerationStructureKHR, 1.f});

  return weights;
};
} // namespace v4dg