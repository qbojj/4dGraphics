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
  };
}

void BindlessManager::UniqueResourceDeleter::operator()(BindlessResource res) const {
  if (!res.valid())
    return;

  if (res)
    m_heap->free(res);
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

  BindlessResource res = m_free.back();
  res.bump_version();

  m_free.pop_back();
  return res;
}

void BindlessManager::BindlessHeap::free(BindlessResource res) {
  if (!res)
    return;

  std::lock_guard lock(m_mutex);
  assert(res.type() == m_type);
  assert(res.index() < m_count);
  
  m_free.push_back(res);
}

BindlessManager::BindlessManager(Handle<Device> device)
    : m_device(device), m_layouts({nullptr, nullptr, nullptr, nullptr}),
      m_pipelineLayout(nullptr), m_pool(nullptr) {

  auto sizes = calculate_sizes(*device);

  for (uint32_t i = 0; i < 4; i++) {
    auto type = static_cast<BindlessType>(i);

    if (type == BindlessType::eAccelerationStructureKHR &&
        !device->m_accelerationStructure)
      continue;

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

  std::array<vk::DescriptorSetLayout, 4> layouts;
  for (uint32_t i{0}; auto &l : m_layouts)
    layouts[i++] = *l;
  m_pipelineLayout = {m_device->device(), {{}, layouts}};

  std::array<vk::DescriptorPoolSize, 4> pool_sizes;
  for (uint32_t i = 0; i < 4; i++) {
    auto type = static_cast<BindlessType>(i);
    pool_sizes[i] = {BindlessResource::type_to_vk(type), sizes[i]};
  }

  m_pool = {m_device->device(), {{}, 4, pool_sizes}};

  auto sets = (*m_device->device())
                  .allocateDescriptorSets({*m_pool, layouts},
                                          *m_device->device().getDispatcher());

  std::move(sets.begin(), sets.end(), m_sets.begin());

  for (uint32_t i = 0; i < 4; i++)
    m_heaps[i].setup(static_cast<BindlessType>(i), sizes[i]);
}

std::array<uint32_t, 4> BindlessManager::calculate_sizes(Device &device) {
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
                            props.limits.maxPerStageDescriptorSamplers});
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
      if (!device.m_accelerationStructure)
        max_count = 0;
      break;
    }

    out[i] = std::min(max_count, BindlessResource::max_count);
  }

  return out;
}
} // namespace v4dg
