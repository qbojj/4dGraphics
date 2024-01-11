#include "v4dgVulkan.hpp"

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <exception>
#include <memory_resource>
#include <typeinfo>

namespace v4dg {
void *vulkan_memory_resource::do_allocate(std::size_t bytes,
                                          std::size_t alignment) {
  return m_allocator.pfnAllocation(m_allocator.pUserData, bytes, alignment,
    static_cast<VkSystemAllocationScope>(vk::SystemAllocationScope::eObject));
}

void vulkan_memory_resource::do_deallocate(void *p, std::size_t, std::size_t) {
  m_allocator.pfnFree(m_allocator.pUserData, p);
}

bool vulkan_memory_resource::do_is_equal(
    const std::pmr::memory_resource &other) const noexcept {
  if (typeid(*this) != typeid(other))
    return false;

  const auto &o_alloc =
      static_cast<const vulkan_memory_resource &>(other).m_allocator;
  return m_allocator.pUserData == o_alloc.pUserData &&
         m_allocator.pfnAllocation == o_alloc.pfnAllocation &&
         m_allocator.pfnFree == o_alloc.pfnFree;
}
} // namespace v4dg