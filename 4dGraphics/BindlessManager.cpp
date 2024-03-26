#include "BindlessManager.hpp"
#include "Device.hpp"
#include "VulkanCaches.hpp"
#include "cppHelpers.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <algorithm>
#include <mutex>
#include <functional>

using namespace v4dg;

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

void BindlessManager::BindlessHeap::setup(BindlessType type,
                                          uint32_t max_count) {
  m_type = type;
  m_max_count = max_count;

  m_count = 1; // null resource
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
  assert(res.index() != 0); // do not free the null resource
  assert(res.index() <= m_count);

  m_free.push_back(std::move(res));
}

BindlessManager::BindlessManager(const Device &device)
    : m_device(&device), m_layout(nullptr), m_pool(nullptr) {

  auto sizes = calculate_sizes(device);

  DescriptorSetLayoutInfo layout_ci;

  for (uint32_t i = 0; i < resource_count; i++) {
    auto type = static_cast<BindlessType>(i);

    if (type == BindlessType::eAccelerationStructureKHR && !device.m_rayTracing)
      continue;

    layout_ci.add_binding(
        i, BindlessResource::type_to_vk(type), vk::ShaderStageFlagBits::eAll,
        sizes[i], {},
        vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending |
            vk::DescriptorBindingFlagBits::ePartiallyBound);
  }

  if (is_debug)
    layout_ci.add_binding(resource_count, vk::DescriptorType::eUniformBuffer,
                          vk::ShaderStageFlagBits::eAll);

  m_layout = layout_ci.create(device);

  std::vector<vk::DescriptorPoolSize> pool_sizes;
  for (uint32_t i = 0; i < resource_count; i++) {
    auto type = static_cast<BindlessType>(i);
    if (type == BindlessType::eAccelerationStructureKHR && !device.m_rayTracing)
      continue;

    pool_sizes.push_back({BindlessResource::type_to_vk(type), sizes[i]});
  }

  if (is_debug)
    pool_sizes.push_back({vk::DescriptorType::eUniformBuffer, 1});

  m_pool = {m_device->device(), {{}, layout_count, pool_sizes}};

  auto sets = (*m_device->device())
                  .allocateDescriptorSets({*m_pool, *m_layout},
                                          *m_device->device().getDispatcher());

  m_set = sets[0];

  m_device->setDebugName(m_set, "bindless set");

  for (uint32_t i = 0; i < resource_count; i++)
    m_heaps[i].setup(static_cast<BindlessType>(i), sizes[i]);

  if (is_debug) {
    size_t whole_size = sizeof(VersionBufferInfo);
    for (uint32_t i = 0; i < resource_count; i++) {
      whole_size = detail::AlignUp(whole_size, 16);
      whole_size += sizes[i] * sizeof(uint8_t);
    }

    std::vector<uint32_t> queue_fam;
    for (const auto &q_fam : device.queues() | std::views::filter(std::not_fn(&std::vector<Queue>::empty))) {
      if (!q_fam.empty()) {
        Queue q = q_fam.front();
        if (q.flags() & (vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eGraphics) &&
            !std::ranges::contains(queue_fam, q.family()))
          queue_fam.push_back(q.family());
      }
    }
    Buffer version_buf{*m_device,
                       whole_size,
                       vk::BufferUsageFlagBits2KHR::eUniformBuffer |
                           vk::BufferUsageFlagBits2KHR::eShaderDeviceAddress,
                       {vma::AllocationCreateFlagBits::eHostAccessRandom |
                            vma::AllocationCreateFlagBits::eMapped,
                        vma::MemoryUsage::eAuto,
                        vk::MemoryPropertyFlagBits::eHostCoherent},
                       {},
                       queue_fam};

    version_buf.setName(*m_device, "bindless version buffer");

    VersionBufferInfo info{};

    auto ai =
        version_buf.allocator().getAllocationInfo(version_buf.allocation());
    std::byte *data = static_cast<std::byte *>(ai.pMappedData);

    size_t offset = 0;
    info.buffers_header =
        reinterpret_cast<VersionBufferHeader *>(data + offset);
    offset += sizeof(VersionBufferHeader);
    for (uint32_t i = 0; i < resource_count; i++) {
      offset = detail::AlignUp(offset, 16);
      info.buffers_header->maxHandles[i] = sizes[i];

      if (sizes[i] == 0) // leave the pointers null
        continue;

      info.buffers_header->versionBuffers[i] =
          version_buf.deviceAddress() + offset;
      info.versionBuffers[i] = reinterpret_cast<uint8_t *>(data + offset);

      std::fill_n(info.versionBuffers[i], sizes[i], 0xffu);

      offset += sizes[i] * sizeof(uint8_t);
    }

    m_versionBuffer = std::pair{std::move(version_buf), info};

    vk::DescriptorBufferInfo buf_info{m_versionBuffer->first, 0,
                                      sizeof(VersionBufferHeader)};
    m_device->device().updateDescriptorSets(
        {vk::WriteDescriptorSet{m_set,
                                resource_count,
                                0,
                                vk::DescriptorType::eUniformBuffer,
                                {},
                                buf_info,
                                {}}},
        {});
  }
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

  size_t max_resources =
      std::min({props.limits.maxPerStageResources,
                props.limits.maxFragmentCombinedOutputResources});

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
  uint32_t type_idx = static_cast<uint32_t>(type);

  assert(type_idx < resource_count && "invalid BindlessType");

  auto &heap = m_heaps[type_idx];
  UniqueBindlessResource resource{heap.allocate(), *this};

  if (m_versionBuffer) {
    uint8_t *values = m_versionBuffer->second.versionBuffers[type_idx];
    values[resource.get().index()] = resource.get().version();
    assert(m_versionBuffer->second.buffers_header->maxHandles[type_idx] >=
           resource.get().index());
  }

  return resource;
}

void BindlessManager::free(BindlessResource res) {
  if (!res)
    return;

  uint32_t type_idx = static_cast<uint32_t>(res.type());

  if (m_versionBuffer) {
    uint8_t *values = m_versionBuffer->second.versionBuffers[type_idx];
    values[res.index()] = 0xffu;
  }

  m_heaps[type_idx].free(std::move(res));
}

vk::WriteDescriptorSet BindlessManager::write_for(BindlessResource res) const {
  assert(res);
  return {
      m_set,
      static_cast<uint32_t>(res.type()), // binding
      res.index(),                       // arrayElement
      1,
      BindlessResource::type_to_vk(res.type()),
      nullptr,
      nullptr,
      nullptr,
  };
}