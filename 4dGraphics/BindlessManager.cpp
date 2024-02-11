#include "BindlessManager.hpp"
#include "Device.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <algorithm>
#include <mutex>

namespace v4dg {
vk::DescriptorType BindlessResource::type_to_vk(BindlessType t) noexcept {
  switch (t) {
  case BindlessType::eSampler:
    return vk::DescriptorType::eSampler;
  case BindlessType::eSampledImage:
    return vk::DescriptorType::eSampledImage;
  case BindlessType::eStorageImage:
    return vk::DescriptorType::eStorageImage;
  case BindlessType::eAccelerationStructureKHR:
    return vk::DescriptorType::eAccelerationStructureKHR;
  default:
    assert(false && "invalid v4dg::BindlessType");
    return {};
  };
}

UniqueBindlessResource::~UniqueBindlessResource() {
  if (m_manager)
    m_manager->free(m_res);
}

void BindlessManager::BindlessHeap::setup(BindlessType type, uint32_t max_count) {
  m_type = type;
  m_max_count = max_count;

  m_count = 0;
  m_free.clear();
}

BindlessResource BindlessManager::BindlessHeap::allocate() {
  std::lock_guard lock(m_mutex);
  if (m_free.empty()) {
    if (m_count >= m_max_count)
      throw exception("out of bindless resources of type {} (max {})", 
        BindlessResource::type_to_vk(m_type), m_max_count);
    
    return BindlessResource{m_count++, m_type};
  }

  BindlessResource res = std::move(m_free.back());
  m_free.pop_back();
  res.bump_version();

  return res;
}

void BindlessManager::BindlessHeap::free(BindlessResource res) {
  if (!res)
    return;

  std::lock_guard lock(m_mutex);
  assert(res.type() == m_type);
  assert(res.index() < m_count);
  
  m_free.push_back(std::move(res));
}

BindlessManager::BindlessManager(const Device &device)
    : m_device(&device), m_layouts({nullptr, nullptr, nullptr, nullptr}),
      m_pool(nullptr) {

  auto sizes = calculate_sizes(device);

  for (uint32_t i = 0; i < 4; i++) {
    auto type = static_cast<BindlessType>(i);

    if (type == BindlessType::eAccelerationStructureKHR &&
        !device.m_rayTracing) {
      m_layouts[i] = {m_device->device(), {{}, {}}};
      continue;
    }

    vk::DescriptorSetLayoutBinding b{0, BindlessResource::type_to_vk(type),
                                     sizes[i], vk::ShaderStageFlagBits::eAll,
                                     nullptr};

    constexpr vk::DescriptorSetLayoutCreateFlags flags = {};

    constexpr vk::DescriptorBindingFlags bindFlags =
        vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending |
        vk::DescriptorBindingFlagBits::ePartiallyBound;

    vk::StructureChain<vk::DescriptorSetLayoutCreateInfo,
                       vk::DescriptorSetLayoutBindingFlagsCreateInfo>
        ci{{flags, b}, {bindFlags}};

    m_layouts[i] = {m_device->device(), ci.get<>()};
  }

  std::array<vk::DescriptorSetLayout, layout_count> layouts;
  for (uint32_t i{0}; auto &l : m_layouts)
    layouts[i++] = *l;

  std::vector<vk::DescriptorPoolSize> pool_sizes;
  for (uint32_t i = 0; i < layout_count; i++) {
    auto type = static_cast<BindlessType>(i);
    if (type == BindlessType::eAccelerationStructureKHR &&
        !device.m_rayTracing)
      continue;
    
    pool_sizes.push_back({BindlessResource::type_to_vk(type), sizes[i]});
  }

  m_pool = {m_device->device(), {{}, layout_count, pool_sizes}};

  auto sets = (*m_device->device())
                  .allocateDescriptorSets({*m_pool, layouts},
                                          *m_device->device().getDispatcher());

  std::move(sets.begin(), sets.end(), m_sets.begin());

  for (uint32_t i = 0; i < (uint32_t)m_sets.size(); i++)
    m_device->setDebugName(m_sets[i], "bindless set {}", 
      BindlessResource::type_to_vk(static_cast<BindlessType>(i)));

  for (uint32_t i = 0; i < 4; i++)
    m_heaps[i].setup(static_cast<BindlessType>(i), sizes[i]);
}

std::array<uint32_t, 4> BindlessManager::calculate_sizes(const Device &device) {
  std::array<uint32_t, 4> out{0, 0, 0, 0};

  auto properties =
      device.physicalDevice()
          .getProperties2<
              vk::PhysicalDeviceProperties2,
              vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();

  auto &props = properties.get<vk::PhysicalDeviceProperties2>().properties;
  auto &accel =
      properties.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();

  // all have update after bind and partially bound

  for (uint32_t i = 0; i < 4; i++) {
    auto type = static_cast<BindlessType>(i);

    uint32_t max_count = 0;
    switch (type) {
    case BindlessType::eSampler:
      max_count = std::min({props.limits.maxDescriptorSetSamplers,
                            props.limits.maxPerStageDescriptorSamplers,
                            props.limits.maxSamplerAllocationCount});
      break;
    case BindlessType::eSampledImage:
      max_count = std::min({props.limits.maxDescriptorSetSampledImages,
                            props.limits.maxPerStageDescriptorSampledImages});
      break;
    case BindlessType::eStorageImage:
      max_count = std::min({props.limits.maxDescriptorSetStorageImages,
                            props.limits.maxPerStageDescriptorStorageImages});
      break;
    case BindlessType::eAccelerationStructureKHR:
      max_count = std::min({accel.maxDescriptorSetAccelerationStructures,
                            accel.maxPerStageDescriptorAccelerationStructures});
      if (!device.m_rayTracing)
        max_count = 0;
      break;
    default:
      assert(false);
    }

    out[i] = std::min(max_count, BindlessResource::max_count);
  }

  size_t total = 0;
  for (auto i : out)
    total += i;
  
  size_t max_resources = std::min({
    props.limits.maxPerStageResources,
    props.limits.maxFragmentCombinedOutputResources
  });

  // use only 4/5 of the total available resources
  if (total > max_resources) {
    for (auto &i : out)
      i = (i * max_resources) / total;
  }

  for (auto [res, i] : std::views::enumerate(out)) {
    logger.Log("BindlessManager ({}): max {} resources",
      BindlessResource::type_to_vk(static_cast<BindlessType>(res)), i);
  }

  return out;
}

UniqueBindlessResource BindlessManager::allocate(BindlessType type) {
  assert(static_cast<uint32_t>(type) < 4 && "invalid BindlessType");
  auto &heap = m_heaps[static_cast<uint32_t>(type)];
  return UniqueBindlessResource{heap.allocate(), *this};
}

void BindlessManager::free(BindlessResource res) {
  auto &heap = m_heaps[static_cast<uint32_t>(res.type())];
  heap.free(std::move(res));
}

vk::WriteDescriptorSet BindlessManager::write_for(BindlessResource res) const {
  assert(res);
  return {
      m_sets[static_cast<uint32_t>(res.type())],
      0, // binding
      res.index(), // arrayElement
      1,
      BindlessResource::type_to_vk(res.type()),
      nullptr,
      nullptr,
      nullptr,
  };
}
} // namespace v4dg
