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
#include <optional>

namespace v4dg {

class DSAllocatorPool;

class DSAllocator {
public:
  DSAllocator(DSAllocatorPool &owner);
  ~DSAllocator();

  template<typename A = std::allocator<vk::DescriptorSet>>
  std::vector<vk::DescriptorSet, A>
  allocate(std::span<const vk::DescriptorSetLayout> setLayouts,
           std::span<const uint32_t> descriptorCounts = {},
           const A &alloc = {}) {
    std::vector<vk::DescriptorSet, A> out(setLayouts.size(), {}, alloc);
    allocate_internal(setLayouts, descriptorCounts, out);
    return out;
  }
  
  vk::DescriptorSet
  allocate(vk::DescriptorSetLayout setLayout,
           std::optional<uint32_t> descriptorCount = {}) {
    vk::DescriptorSet out{};
    std::span<const uint32_t> descriptorCounts{};
    if (descriptorCount)
      descriptorCounts = {{*descriptorCount}};
    allocate_internal({{setLayout}}, descriptorCounts, {&out,1});
    return out;
  }

private:
  void allocate_internal(std::span<const vk::DescriptorSetLayout> setLayouts,
                         std::span<const uint32_t> descriptorCounts,
                         std::span<vk::DescriptorSet> out);
  DSAllocatorPool *m_owner;
  vk::raii::DescriptorPool m_pool;
};

struct DSAllocatorWeights{
  struct DescriptorWieght{
    vk::DescriptorType type;
    float weight;
  };

  std::vector<DescriptorWieght> m_weights{};

  // data weight is in m_weights
  float m_inlineUniformBindingWeight{0.0f};

  std::vector<std::vector<vk::DescriptorType>> m_mutableTypeLists{};

  vk::raii::DescriptorPool create(const vk::raii::Device &device,
                                  std::uint32_t maxSets,
                                  vk::DescriptorPoolCreateFlags flags = {}) const;
};

class DSAllocatorPool {
public:
  DSAllocatorPool(const vk::raii::Device &device,
                  DSAllocatorWeights weights);

  void advance_frame(vk::Bool32 trim = vk::False,
                     vk::DescriptorPoolResetFlags ResetFlags = {});

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

  const vk::raii::Device &m_device;
  DSAllocatorWeights m_weights;

  std::mutex m_mut;
  std::uint32_t m_frameIdx{0};

  per_frame<frame_storage> m_perFramePools; // destruction queue
  dpvec m_cleanPools;
};
} // namespace v4dg