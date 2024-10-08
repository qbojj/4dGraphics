#include "v4dgVulkan.hpp"

#include "cppHelpers.hpp"

#include <vulkan/vulkan.hpp>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory_resource>
#include <utility>
#include <variant>

using namespace v4dg;
void *vulkan_memory_resource::do_allocate(std::size_t bytes,
                                          std::size_t alignment) {
  return m_allocator.pfnAllocation(
      m_allocator.pUserData, bytes, alignment,
      static_cast<VkSystemAllocationScope>(vk::SystemAllocationScope::eObject));
}

void vulkan_memory_resource::do_deallocate(void *p, std::size_t /*__bytes*/,
                                           std::size_t /*__alignment*/) {
  m_allocator.pfnFree(m_allocator.pUserData, p);
}

bool vulkan_memory_resource::do_is_equal(
    const std::pmr::memory_resource &other) const noexcept {
  if (typeid(*this) != typeid(other)) {
    return false;
  }

  const auto &o_alloc =
      dynamic_cast<const vulkan_memory_resource &>(other).m_allocator;
  return m_allocator.pUserData == o_alloc.pUserData &&
         m_allocator.pfnAllocation == o_alloc.pfnAllocation &&
         m_allocator.pfnFree == o_alloc.pfnFree;
}

vk::BufferUsageFlags2KHR v4dg::getBufferUsage(const vk::BufferCreateInfo &bci) {
  if (const auto *usage =
          getVkStructureFromChain<vk::BufferUsageFlags2CreateInfoKHR>(&bci)) {
    return usage->usage;
  }
  return static_cast<vk::BufferUsageFlags2KHR>(uint32_t{bci.usage});
}

// NOLINTNEXTLINE(bugprone-exception-escape): false positive
DestructionItem::~DestructionItem() {
  if (item.valueless_by_exception()) {
    return;
  }

  std::visit(detail::overload_set{
                 [](std::monostate) noexcept {},
                 [](fun_t &f) noexcept {
                   if (f) {
                     f();
                   }
                 },
                 [](ptr_t &p) noexcept { p.reset(); },
             },
             item);
}

DestructionItem &DestructionItem::operator=(DestructionItem &&o) noexcept {
  DestructionItem it{std::move(o)};
  std::swap(item, it.item);
  return *this;
}

void DestructionStack::append(DestructionStack &&o) {
  auto &&src = std::move(o).m_stack;
  m_stack.insert(m_stack.end(), std::make_move_iterator(src.begin()),
                 std::make_move_iterator(src.end()));
  src.clear();
}
