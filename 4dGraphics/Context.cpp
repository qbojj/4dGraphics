#include "Context.hpp"

#include "Debug.hpp"
#include "CommandBufferManager.hpp"
#include "v4dgCore.hpp"

#include <tracy/Tracy.hpp>
#include <taskflow/taskflow.hpp>

#include <bit>
#include <ranges>
#include <vector>
#include <exception>

namespace v4dg {
namespace {
  class WorkerInterface : public tf::WorkerInterface { 
  public:
     void scheduler_prologue(tf::Worker& worker) override {
        // set thread name
        std::string name = "v4dg worker " + std::to_string(worker.id());
        tracy::SetThreadName(name.c_str());
     }

     void scheduler_epilogue(tf::Worker&, std::exception_ptr) override {
        // do nothing
     }
  };
}
PerQueueFamily::PerQueueFamily(Handle<Queue> queue, const vk::raii::Device &dev)
    : m_queue(std::move(queue)), m_semaphore(nullptr),
      m_command_buffer_manager(dev, m_queue->family()) {
  vk::StructureChain<vk::SemaphoreCreateInfo, vk::SemaphoreTypeCreateInfo> sci{
      {}, {vk::SemaphoreType::eTimeline, 0}};

  m_semaphore = {dev, sci.get<>()};
}

Context::Context(Handle<Instance> instance, Handle<Device> device)
    : m_instance(std::move(instance)), m_device(std::move(device)),
      m_executor(std::thread::hardware_concurrency(),
        std::make_shared<WorkerInterface>()),
      m_main_thread_id(std::this_thread::get_id()),
      m_families(getFamilies()),
      m_per_frame{make_per_frame<PerFrame>(vkDevice(), m_families.size())},
      m_pipeline_cache(nullptr) {
  auto &graphics_queue = get_queue(PerQueueFamily::Type::Graphics);
  uint32_t graphics_family = graphics_queue->queue()->family();

  m_per_thread.reserve(m_executor.num_workers());
  for (size_t i{}; i < m_executor.num_workers(); ++i)
    m_per_thread.emplace_back(vkDevice(), graphics_family);

  auto pipeline_cache_data = getPipelineCacheData();
  m_pipeline_cache = vkDevice().createPipelineCache(
      {{}, pipeline_cache_data.size(), pipeline_cache_data.data()});
}

Context::~Context() {
  try {
    auto data = m_pipeline_cache.getData();
    savePipelineCacheData({reinterpret_cast<std::byte*>(data.data()), data.size() * sizeof(data[0])});
  } catch (const std::exception &e) {
    logger.Error("failed to save pipeline cache ({})", e.what());
  }
}

auto Context::getFamilies() const -> PerQueueFamilyArray {
  static constexpr auto N = PerQueueFamilyArray{}.size();
  std::array<Handle<Queue>, N> families_queues;

  auto to_idx = [](QueueType type) { return static_cast<uint32_t>(type); };

  auto has_all_flags = [](vk::QueueFlags flags, vk::QueueFlags required) {
    return (flags & required) == required;
  };

  auto flag_count = [](vk::QueueFlags flags) {
    return std::popcount(static_cast<uint32_t>(flags));
  };

  for (auto &queue_fam : m_device->queues()) {
    if (queue_fam.empty())
      continue;

    size_t idx = 0;
    auto queue = queue_fam[idx];

    // find queue families with least flags
    for (auto [type, required_flags] : PerQueueFamily::QueueTypes) {
      if (!has_all_flags(queue->flags(), required_flags))
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
  auto &cur_frame = get_frame_ctx();

  for (auto [i, q] : std::views::enumerate(m_families)) {
    cur_frame.m_semaphore_ready_values[i] = q ? q->semaphoreValue() : 0;
  }

  // we want to wait for every resource from previous 'next' frame to be not
  // used
  m_frame_idx++;
  auto &next_frame = get_frame_ctx();

  std::vector<vk::Semaphore> semaphores;
  std::vector<uint64_t> values;
  for (auto [i, q] : std::views::enumerate(m_families)) {
    if (q) {
      semaphores.push_back(*q->semaphore());
      values.push_back(next_frame.m_semaphore_ready_values[i]);
    }
  }

  {
    ZoneScopedN("wait for semaphores");
    auto result = vkDevice().waitSemaphores({{}, semaphores, values},
                                            std::numeric_limits<uint64_t>::max());
    if (result != vk::Result::eSuccess)
      throw exception("waitSemaphores failed: {}", result);
  }

  ZoneScopedN("clear destruction stacks");
  cur_frame.m_destruction_stack.flush();

  for (auto &per_thread : m_per_thread)
    per_thread.m_per_frame[frame_ref()].m_destruction_stack.flush();

  for (auto &q : m_families)
    if (q) {
      q->commandBufferManager().reset();
    }
}

std::vector<std::byte> Context::getPipelineCacheData() {
  // load pipeline cache from file
  std::vector<std::byte> data;
  std::ifstream file("pipeline_cache.bin", std::ios::binary);
  if (file) {
    file.seekg(0, std::ios::end);
    data.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(data.data()), data.size());
  }
  
  return data;
}

void Context::savePipelineCacheData(std::span<const std::byte> data) {
  // save pipeline cache to file
  std::ofstream file("pipeline_cache.bin", std::ios::binary);
  if (file)
    file.write(reinterpret_cast<const char *>(data.data()), data.size());
}
} // namespace v4dg