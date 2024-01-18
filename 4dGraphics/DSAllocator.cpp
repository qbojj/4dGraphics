#include "DSAllocator.hpp"

#include "Debug.hpp"
#include "Device.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <mutex>
#include <algorithm>
#include <cassert>
#include <utility>

namespace v4dg {
DSAllocator::DSAllocator(DSAllocatorPool &owner) : m_owner(&owner), m_pool(nullptr) {}
DSAllocator::~DSAllocator() { m_owner->ret_allocator(std::move(m_pool)); }

std::vector<vk::DescriptorSet>
DSAllocator::allocate(std::span<const vk::DescriptorSetLayout> setLayouts,
                      std::span<const uint32_t> descriptorCounts) {
  assert(m_owner);

  if (!descriptorCounts.empty() && descriptorCounts.size() != setLayouts.size())
    throw std::invalid_argument("v4dg::DSAllocator::allocate: "
                                "descriptorCounts.size() != setLayouts.size()");

  std::vector<vk::DescriptorSet> out(setLayouts.size());

  vk::Result res{};

  auto device = m_pool.getDevice();
  auto *disp = m_pool.getDispatcher();

  if (!*m_pool)
    m_pool = m_owner->get_new_pool();

  for (uint32_t i = 0; i < 64; i++) {
    vk::StructureChain<vk::DescriptorSetAllocateInfo,
                       vk::DescriptorSetVariableDescriptorCountAllocateInfo>
        createInfo{{*m_pool, setLayouts}, {descriptorCounts}};

    res = device.allocateDescriptorSets(&createInfo.get<>(), out.data(), *disp);

    if (res == vk::Result::eSuccess)
      return out;

    // is it out of pool memory error?
    vk::resultCheck(
        res, "v4dg::DSAllocator::allocate",
        {vk::Result::eErrorFragmentedPool, vk::Result::eErrorOutOfPoolMemory});

    // pool is full - try again
    m_owner->replace_full_allocator(m_pool);
  }

  // we failed multiple times. something is very wrong.
  vk::resultCheck(res, "v4dg::DSAllocator::allocate");
  assert(0);
  return {};
}

DSAllocatorPool::DSAllocatorPool(
    Handle<Device> device,
    std::shared_ptr<vk::DescriptorPoolCreateInfo> createInfo,
    std::uint32_t framesInFlight)
    : m_device(std::move(device)), m_createInfo(std::move(createInfo)),
      m_framesInFlight(0) {
  set_frames_in_flight(framesInFlight);
}

void DSAllocatorPool::set_frames_in_flight(std::uint32_t framesInFlight) {
  if (framesInFlight > max_frames_in_flight)
    throw std::invalid_argument("v4dg::DSAllocatorPool::set_frames_in_flight: "
                                "framesInFlight > max_frames_in_flight");

  std::unique_lock lock(m_mut);

  if (framesInFlight == m_framesInFlight)
    return;

  // reorder pools as if they started from the current frame
  if (m_frameIdx != 0) {
    std::rotate(m_perFramePools.begin(), m_perFramePools.begin() + m_frameIdx,
                m_perFramePools.begin() + framesInFlight);
    m_frameIdx = 0;
  }

  // move pools after the new framesInFlight to current (must wait the longest)
  auto &dst = m_perFramePools.front();

  for (std::uint32_t i = framesInFlight; i < m_framesInFlight; i++) {
    frame_storage &storage = m_perFramePools[i];
    for (auto &pool : storage.usable)
      dst.usable.emplace_back(std::move(pool));
    for (auto &pool : storage.full)
      dst.full.emplace_back(std::move(pool));

    storage.usable.clear();
    storage.full.clear();
  }

  m_framesInFlight = framesInFlight;
}

void DSAllocatorPool::advance_frame(vk::Bool32 trim,
                                    vk::DescriptorPoolResetFlags ResetFlags) {
  std::unique_lock lock(m_mut);

  if (trim)
    m_cleanPools.clear();

  frame_storage &storage = m_perFramePools[m_frameIdx];

  for (auto &pool : storage.full)
    pool.reset(ResetFlags);
  for (auto &pool : storage.usable)
    pool.reset(ResetFlags);

  m_cleanPools.insert(m_cleanPools.end(),
                      std::move_iterator(storage.full.begin()),
                      std::move_iterator(storage.full.end()));

  storage.full.clear();
  storage.usable.clear();

  if (trim) {
    m_cleanPools.shrink_to_fit();
    storage.full.shrink_to_fit();
    storage.usable.shrink_to_fit();
  };
}

void DSAllocatorPool::ret_allocator(vk::raii::DescriptorPool pool) {
  std::unique_lock lock(m_mut);
  m_perFramePools[m_frameIdx].usable.emplace_back(std::move(pool));
}

void DSAllocatorPool::replace_full_allocator(vk::raii::DescriptorPool &pool) {
  std::unique_lock lock(m_mut);

  frame_storage &storage = m_perFramePools[m_frameIdx];
  storage.full.emplace_back(std::move(pool));
  pool = get_new_pool_internal();
}

vk::raii::DescriptorPool DSAllocatorPool::get_new_pool() {
  std::unique_lock lock(m_mut);
  return get_new_pool_internal();
}

vk::raii::DescriptorPool DSAllocatorPool::get_new_pool_internal() {
  frame_storage &storage = m_perFramePools[m_frameIdx];

  vk::raii::DescriptorPool pool(nullptr);

  if (!storage.usable.empty()) {
    pool = std::move(storage.usable.back());
    storage.usable.pop_back();
  } else if (!m_cleanPools.empty()) {
    pool = std::move(m_cleanPools.back());
    m_cleanPools.pop_back();
  } else {
    pool = m_device->device().createDescriptorPool(*m_createInfo);
  }

  return pool;
}
} // namespace v4dg