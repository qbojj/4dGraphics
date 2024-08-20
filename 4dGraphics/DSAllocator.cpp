#include "DSAllocator.hpp"

#include "v4dgCore.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cassert>
#include <cstdint>
#include <iterator>
#include <mutex>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace v4dg {
DSAllocator::DSAllocator(DSAllocatorPool &owner)
    : m_owner(&owner), m_pool(nullptr) {}
DSAllocator::~DSAllocator() {
  if (*m_pool) {
    m_owner->ret_allocator(std::move(m_pool));
  }
}

void DSAllocator::allocate_internal(
    std::span<const vk::DescriptorSetLayout> setLayouts,
    std::span<const uint32_t> descriptorCounts,
    std::span<vk::DescriptorSet> out) {
  assert(m_owner);

  if (!descriptorCounts.empty() &&
      descriptorCounts.size() != setLayouts.size()) {
    throw std::invalid_argument("v4dg::DSAllocator::allocate: "
                                "descriptorCounts.size() != setLayouts.size()");
  }

  if (!*m_pool) {
    m_pool = m_owner->get_new_pool();
  }

  auto device = m_pool.getDevice();
  const auto *disp = m_pool.getDispatcher();

  static constexpr auto max_tries = 64;

  vk::Result res{};
  for (int i = 0; i < max_tries; i++) {
    vk::StructureChain<vk::DescriptorSetAllocateInfo,
                       vk::DescriptorSetVariableDescriptorCountAllocateInfo>
        createInfo{{*m_pool, setLayouts}, {descriptorCounts}};

    res = device.allocateDescriptorSets(&createInfo.get<>(), out.data(), *disp);

    if (res == vk::Result::eSuccess) {
      return;
    }

    // is it out of pool memory error?
    vk::detail::resultCheck(
        res, "v4dg::DSAllocator::allocate",
        {vk::Result::eErrorFragmentedPool, vk::Result::eErrorOutOfPoolMemory});

    // pool is full - try again
    m_owner->replace_full_allocator(m_pool);
  }

  // we failed multiple times. something is very wrong.
  vk::detail::resultCheck(res, "v4dg::DSAllocator::allocate");
  assert(false);
}

vk::raii::DescriptorPool
DSAllocatorWeights::create(const vk::raii::Device &device,
                           std::uint32_t maxSets,
                           vk::DescriptorPoolCreateFlags flags) const {
  auto to_count = [&](float weight) {
    return static_cast<std::uint32_t>(weight * static_cast<float>(maxSets));
  };

  std::vector<vk::DescriptorPoolSize> sizes{};
  sizes.reserve(m_weights.size());
  for (const auto &weight : m_weights) {
    sizes.emplace_back(weight.type, to_count(weight.weight));
  }

  std::vector<vk::MutableDescriptorTypeListVALVE> mutableTypeLists{};
  mutableTypeLists.reserve(m_mutableTypeLists.size());
  for (const auto &list : m_mutableTypeLists) {
    mutableTypeLists.emplace_back(list);
  }

  vk::StructureChain<vk::DescriptorPoolCreateInfo,
                     vk::DescriptorPoolInlineUniformBlockCreateInfo,
                     vk::MutableDescriptorTypeCreateInfoEXT>
      chain{{flags, maxSets, sizes},
            {to_count(m_inlineUniformBindingWeight)},
            {mutableTypeLists}};

  if (m_inlineUniformBindingWeight == 0.0F) {
    chain.unlink<vk::DescriptorPoolInlineUniformBlockCreateInfo>();
  }

  if (mutableTypeLists.empty()) {
    chain.unlink<vk::MutableDescriptorTypeCreateInfoEXT>();
  }

  return {device, chain.get<>()};
}

DSAllocatorPool::DSAllocatorPool(const vk::raii::Device &device,
                                 DSAllocatorWeights weights)
    : m_device(device), m_weights(std::move(weights)) {}

void DSAllocatorPool::advance_frame(vk::Bool32 trim,
                                    vk::DescriptorPoolResetFlags ResetFlags) {
  std::scoped_lock const lock(m_mut);

  m_frameIdx = (m_frameIdx + 1) % max_frames_in_flight;

  if (trim != 0U) {
    m_cleanPools.clear();
  }

  frame_storage &storage = m_perFramePools[m_frameIdx];

  for (auto &pool : storage.full) {
    pool.reset(ResetFlags);
  }
  for (auto &pool : storage.usable) {
    pool.reset(ResetFlags);
  }

  m_cleanPools.insert(m_cleanPools.end(),
                      std::move_iterator(storage.full.begin()),
                      std::move_iterator(storage.full.end()));

  storage.full.clear();
  storage.usable.clear();

  if (trim != 0U) {
    m_cleanPools.shrink_to_fit();
    storage.full.shrink_to_fit();
    storage.usable.shrink_to_fit();
  };
}

void DSAllocatorPool::ret_allocator(vk::raii::DescriptorPool pool) {
  std::scoped_lock const lock(m_mut);
  m_perFramePools[m_frameIdx].usable.emplace_back(std::move(pool));
}

void DSAllocatorPool::replace_full_allocator(vk::raii::DescriptorPool &pool) {
  std::scoped_lock const lock(m_mut);

  frame_storage &storage = m_perFramePools[m_frameIdx];
  storage.full.emplace_back(std::move(pool));
  pool = get_new_pool_internal();
}

vk::raii::DescriptorPool DSAllocatorPool::get_new_pool() {
  std::scoped_lock const lock(m_mut);
  return get_new_pool_internal();
}

vk::raii::DescriptorPool DSAllocatorPool::get_new_pool_internal() {
  frame_storage &storage = m_perFramePools[m_frameIdx];

  vk::raii::DescriptorPool pool(nullptr);

  static constexpr int d_sets_per_pool = 1024 * 2;

  if (!storage.usable.empty()) {
    pool = std::move(storage.usable.back());
    storage.usable.pop_back();
  } else if (!m_cleanPools.empty()) {
    pool = std::move(m_cleanPools.back());
    m_cleanPools.pop_back();
  } else {
    pool = m_weights.create(m_device, d_sets_per_pool);
  }

  return pool;
}
} // namespace v4dg
