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
PerQueueFamily::PerQueueFamily(Handle<Queue> queue, const vk::raii::Device &dev)
    : m_queue(std::move(queue)), m_semaphore(nullptr),
      m_command_buffer_managers(make_per_frame<command_buffer_manager>(dev, m_queue->family())) {
  vk::StructureChain<vk::SemaphoreCreateInfo, vk::SemaphoreTypeCreateInfo> sci{
      {}, {vk::SemaphoreType::eTimeline, 0}};

  m_semaphore = {dev, sci.get<>()};
}

Context::Context(const Device &dev, DSAllocatorWeights weights)
    : m_instance(dev.instance()), m_device(dev),
      m_executor(std::thread::hardware_concurrency(),
                 std::make_shared<WorkerInterface>()),
      m_main_thread_id(std::this_thread::get_id()), m_families(getFamilies()),
      m_per_frame{
          make_per_frame<PerFrame>(vkDevice(), m_families.size(), weights)},
      m_pipeline_cache(nullptr) //,m_bindless_manager(device())
      {
  auto &graphics_queue = get_queue(PerQueueFamily::Type::Graphics);
  uint32_t graphics_family = graphics_queue->queue()->family();

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
  static constexpr auto N = std::size(PerQueueFamilyArray{});
  std::array<Handle<Queue>, N> families_queues;

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

  for (auto &queue_fam : device().queues()) {
    if (queue_fam.empty())
      continue;

    size_t idx = 0;
    auto queue = queue_fam[idx];

    // find queue families with least flags
    for (auto [type, required_flags, banned_flags] :
         PerQueueFamily::QueueTypes) {
      if (!has_all_flags(queue->flags(), required_flags) ||
          has_any_flags(queue->flags(), banned_flags))
        continue;

      auto type_idx = to_idx(type);
      if (families_queues[type_idx] &&
          flag_count(families_queues[type_idx]->flags()) <=
              flag_count(queue->flags()))
        continue;

      families_queues[type_idx] = std::move(queue);

      idx++;
      if (idx == queue_fam.size())
        break;

      queue = queue_fam[idx];
    }
  }

  auto &async_queue = families_queues[to_idx(QueueType::AsyncCompute)];

  // asnyc compute should not have graphics capabilities
  if (async_queue &&
      has_all_flags(async_queue->flags(), vk::QueueFlagBits::eGraphics))
    async_queue = nullptr;

  if (!families_queues[to_idx(QueueType::Graphics)])
    throw exception("cannot find omni queue");

  PerQueueFamilyArray array;
  for (auto [i, q] : std::views::enumerate(families_queues))
    if (q)
      array[i] = PerQueueFamily(std::move(q), vkDevice());

  return array;
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

    for (auto &per_thread : m_per_thread) {
      auto &per_f = per_thread.m_per_frame[frame_ref()];
      per_f.m_destruction_stack.flush();
      per_f.m_command_buffer_manager.reset();
    }

    for (auto &q : m_families)
      if (q) {
        q->commandBufferManager(frame_ref()).reset();
      }
  }
}
} // namespace v4dg