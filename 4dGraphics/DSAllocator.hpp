#pragma once

#include "Device.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace v4dg {

class DSAllocatorPool;

class DSAllocator {
public:
  DSAllocator(DSAllocatorPool &owner);
  ~DSAllocator();

  std::vector<vk::DescriptorSet>
  allocate(std::span<const vk::DescriptorSetLayout> setLayouts,
           std::span<const uint32_t> descriptorCounts = {});

private:
  DSAllocatorPool *m_owner;
  vk::raii::DescriptorPool m_pool;
};

class DSAllocatorPool {
public:
  DSAllocatorPool(Handle<Device> device,
                  std::shared_ptr<vk::DescriptorPoolCreateInfo> createInfo,
                  std::uint32_t framesInFlight = max_frames_in_flight);

  void advance_frame(vk::Bool32 trim = vk::False,
                     vk::DescriptorPoolResetFlags ResetFlags = {});

  void set_frames_in_flight(std::uint32_t framesInFlight);

  DSAllocator get_allocator() { return {*this}; }

  // This is a raw interface. You should use DSAllocator instead so used pools
  // go back to the pool and are not destroyed and recycled.
  vk::raii::DescriptorPool get_new_pool();
  void replace_full_allocator(vk::raii::DescriptorPool &pool);
  void ret_allocator(vk::raii::DescriptorPool pool);

private:
  vk::raii::DescriptorPool get_new_pool_internal();

  using dpvec = std::vector<vk::raii::DescriptorPool>;

  struct frame_storage {
    dpvec usable;
    dpvec full;
  };

  Handle<Device> m_device;
  std::shared_ptr<vk::DescriptorPoolCreateInfo> m_createInfo;

  std::mutex m_mut;
  std::uint32_t m_frameIdx{0};

  std::uint32_t m_framesInFlight;
  per_frame<frame_storage> m_perFramePools;
  dpvec m_cleanPools;

  // stats (10 frame moving average)
  std::uint32_t m_totalPools, m_allUsedPools;
};
} // namespace v4dg