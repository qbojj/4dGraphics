#include "BindlessManager.hpp"
#include "Debug.hpp"
#include "Device.hpp"
#include "Queue.hpp"
#include "VulkanCaches.hpp"
#include "VulkanConstructs.hpp"
#include "cppHelpers.hpp"
#include "v4dgCore.hpp"

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <ranges>
#include <utility>
#include <vector>

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
  if (m_manager != nullptr) {
    m_manager->free(m_res);
  }
}

void BindlessManager::BindlessHeap::setup(BindlessType type,
                                          uint32_t max_count) {
  m_type = type;
  m_max_count = max_count;

  m_count = 1; // null resource
  m_free.clear();
}

BindlessResource BindlessManager::BindlessHeap::allocate() {
  std::scoped_lock const _{m_mutex};
  if (m_free.empty()) {
    if (m_count >= m_max_count) {
      throw exception("out of bindless resources of type {} (max {})",
                      BindlessResource::type_to_vk(m_type), m_max_count);
    }

    return BindlessResource{m_count++, m_type};
  }

  BindlessResource res = m_free.back();
  m_free.pop_back();
  res.bump_version();

  return res;
}

void BindlessManager::BindlessHeap::free(BindlessResource res) {
  if (!res) {
    return;
  }

  std::scoped_lock const _{m_mutex};
  assert(res.type() == m_type);
  assert(res.index() != 0); // do not free the null resource
  assert(res.index() <= m_count);

  m_free.push_back(res);
}

BindlessManager::BindlessManager(const Device &device)
    : m_device(&device), m_layout(nullptr), m_pool(nullptr) {

  auto sizes = calculate_sizes(device);

  DescriptorSetLayoutInfo layout_ci;

  for (auto [i, type, size] :
       std::views::zip(std::views::iota(0U), resource_types, sizes)) {
    if (type == BindlessType::eAccelerationStructureKHR &&
        !device.stats().has_extension(
            vk::KHRAccelerationStructureExtensionName)) {
      continue;
    }

    layout_ci.add_binding(
        i, BindlessResource::type_to_vk(type), vk::ShaderStageFlagBits::eAll,
        size, {},
        vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending |
            vk::DescriptorBindingFlagBits::ePartiallyBound);
  }

  if (is_debug) {
    layout_ci.add_binding(resource_count, vk::DescriptorType::eUniformBuffer,
                          vk::ShaderStageFlagBits::eAll);
  }

  m_layout = layout_ci.create(device);

  std::vector<vk::DescriptorPoolSize> pool_sizes;
  for (auto [type, size] : std::views::zip(resource_types, sizes)) {
    if (type == BindlessType::eAccelerationStructureKHR &&
        !device.stats().has_extension(
            vk::KHRAccelerationStructureExtensionName)) {
      continue;
    }

    pool_sizes.emplace_back(BindlessResource::type_to_vk(type), size);
  }

  if (is_debug) {
    pool_sizes.emplace_back(vk::DescriptorType::eUniformBuffer, 1);
  }

  m_pool = {m_device->device(), {{}, layout_count, pool_sizes}};

  auto sets = (*m_device->device())
                  .allocateDescriptorSets({*m_pool, *m_layout},
                                          *m_device->device().getDispatcher());

  m_set = sets[0];

  m_device->setDebugName(m_set, "bindless set");

  for (auto &&[heap, type, size] :
       std::views::zip(m_heaps, resource_types, sizes)) {
    heap.setup(type, size);
  }

  if (!is_debug) {
    return;
  }

  static constexpr auto size_alignment = 16;

  std::size_t whole_size = sizeof(VersionBufferInfo);
  for (std::uint32_t const size : sizes) {
    whole_size = AlignUp(whole_size, size_alignment);
    whole_size += size * sizeof(std::uint8_t);
  }

  std::vector<std::uint32_t> queue_fam;
  for (const auto &q_fam : device.queues() | std::views::filter(std::not_fn(
                                                 &std::vector<Queue>::empty))) {
    if (!q_fam.empty()) {
      const Queue &q = q_fam.front();
      if (q.flags() &
              (vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eGraphics) &&
          !std::ranges::contains(queue_fam, q.family())) {
        queue_fam.push_back(q.family());
      }
    }
  }

  Buffer version_buf{
      *m_device,
      whole_size,
      vk::BufferUsageFlagBits2KHR::eUniformBuffer |
          vk::BufferUsageFlagBits2KHR::eShaderDeviceAddress,
      {
          vma::AllocationCreateFlagBits::eHostAccessRandom |
              vma::AllocationCreateFlagBits::eMapped,
          vma::MemoryUsage::eAuto,
          vk::MemoryPropertyFlagBits::eHostCoherent,
      },
      {},
      queue_fam,
  };

  version_buf->setName(*m_device, "bindless version buffer");

  VersionBufferInfo info{};

  auto ai =
      version_buf->allocator().getAllocationInfo(version_buf->allocation());
  auto *data = static_cast<std::byte *>(ai.pMappedData);

  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  info.buffers_header = new (data) VersionBufferHeader;
  std::ranges::copy(sizes, info.buffers_header->maxHandles.begin());

  auto offset = sizeof(VersionBufferHeader);
  for (auto [size, version_buf_device_addr, version_buf_data] :
       std::views::zip(sizes, std::span{info.buffers_header->versionBuffers},
                       std::span{info.versionBuffers})) {
    offset = AlignUp(offset, size_alignment);

    if (size == 0) { // leave the pointers null
      version_buf_device_addr = vk::DeviceAddress{};
      continue;
    }

    version_buf_device_addr = version_buf->deviceAddress() + offset;

    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    version_buf_data = new (data + offset) std::uint8_t[size];
    std::fill_n(version_buf_data, size,
                std::numeric_limits<std::uint8_t>::max());

    offset += size * sizeof(uint8_t);
  }

  m_versionBuffer = {.buffer = std::move(version_buf), .info = info};

  vk::DescriptorBufferInfo const buf_info{
      m_versionBuffer->buffer->vk(),
      0,
      sizeof(VersionBufferHeader),
  };
  m_device->device().updateDescriptorSets(
      {
          vk::WriteDescriptorSet{
              m_set,
              resource_count,
              0,
              vk::DescriptorType::eUniformBuffer,
              {},
              buf_info,
              {},
          },
      },
      {});
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

  for (auto &&[type, size] : std::views::zip(resource_types, out)) {
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
      if (!device.stats().has_extension(
              vk::KHRAccelerationStructureExtensionName)) {
        max_count = 0;
      }
      break;
    default:
      assert(false);
    }

    size = std::min(max_count, BindlessResource::max_count);
  }

  auto total = std::ranges::fold_left(out, 0U, std::plus<>{});

  static constexpr float max_bindless_factor = 0.8F;
  auto max_bindless = static_cast<std::uint32_t>(
      static_cast<float>(props.limits.maxPerStageResources) *
      max_bindless_factor);

  // use only 4/5 of the total available resources
  if (total > max_bindless) {
    // remove the smallest values first
    std::array indices{0, 1, 2, 3};
    std::ranges::sort(indices, std::greater<>{},
                      [&out](auto i) { return out[i]; });

    for (int i = 0; i < 4; i++) {
      if (total <= max_bindless) {
        break;
      }

      auto dec_floor = i < 3 ? out[indices[i + 1]] : 0U;

      auto to_distribute = std::min((out[indices[i]] - dec_floor) * (i + 1),
                                    total - max_bindless + i);

      // remove to_distribute equally from all the resources idx<=i
      auto dec = to_distribute / (i + 1);

      for (auto idx : indices | std::views::take(i + 1)) {
        out[idx] -= dec;
      }

      total -= (i + 1) * dec;
    }
  }

  for (auto [res, type] : std::views::zip(out, resource_types)) {
    logger.Log("BindlessManager ({}): max {} resources",
               BindlessResource::type_to_vk(type), res);
  }

  return out;
}

UniqueBindlessResource BindlessManager::allocate(BindlessType type) {
  auto type_idx = static_cast<uint32_t>(type);

  assert(type_idx < resource_count && "invalid BindlessType");

  auto &heap = m_heaps[type_idx];
  UniqueBindlessResource resource{heap.allocate(), *this};

  if (m_versionBuffer) {
    uint8_t *values = m_versionBuffer->info.versionBuffers[type_idx];
    values[resource.get().index()] = resource.get().version();
    assert(m_versionBuffer->info.buffers_header->maxHandles[type_idx] >=
           resource.get().index());
  }

  return resource;
}

void BindlessManager::free(BindlessResource res) noexcept {
  if (!res) {
    return;
  }

  auto type_idx = static_cast<std::uint32_t>(res.type());

  if (type_idx >= resource_count) {
    assert(false && "invalid BindlessResource type");
    return;
  }

  if (m_versionBuffer) {
    uint8_t *values = m_versionBuffer->info.versionBuffers[type_idx];
    assert(res.index() <
               m_versionBuffer->info.buffers_header->maxHandles[type_idx] &&
           "invalid BindlessResource index");

    values[res.index()] = std::numeric_limits<uint8_t>::max();
  }

  m_heaps[type_idx].free(res);
}

vk::WriteDescriptorSet BindlessManager::write_for(BindlessResource res) const {
  assert(res);
  return {
      m_set,
      static_cast<std::uint32_t>(res.type()), // binding
      res.index(),                            // arrayElement
      1,
      BindlessResource::type_to_vk(res.type()),
      nullptr,
      nullptr,
      nullptr,
  };
}
