#include "DynamicStructureChain.hpp"

#include <vulkan/vulkan.hpp>

#include <cstdint>

using namespace v4dg;

detail::DynamicStructureChainBase::DynamicStructureChainBase(
    const DynamicStructureChainBase &o) {
  dynamic_data = o.dynamic_data;
  fixup(o.dynamic_data.data());
}

auto detail::DynamicStructureChainBase::operator=(
    const DynamicStructureChainBase &o) -> DynamicStructureChainBase & {
  dynamic_data = o.dynamic_data;
  fixup(o.dynamic_data.data());
  return *this;
}

void *detail::DynamicStructureChainBase::expand_storage(std::size_t size) {
  void *old_data = data();
  std::size_t old_size = dynamic_data.size();
  dynamic_data.insert(dynamic_data.end(), size, std::byte{});
  fixup(old_data);

  void *new_data = dynamic_data.data() + old_size;

  if (old_size > 0) {
    auto *last = reinterpret_cast<vk::BaseOutStructure *>(data());
    while (last->pNext)
      last = last->pNext;
    last->pNext = reinterpret_cast<vk::BaseOutStructure *>(new_data);
  }

  return new_data;
}

void detail::DynamicStructureChainBase::fixup(const void *old_ptr) noexcept {
  void *cur_ptr = data();
  if (cur_ptr == old_ptr)
    return;

  std::ptrdiff_t offset = static_cast<std::byte *>(cur_ptr) -
                          static_cast<const std::byte *>(old_ptr);
  auto *el = reinterpret_cast<vk::BaseOutStructure *>(cur_ptr);
  while (el) {
    if (el->pNext) {
      el->pNext = reinterpret_cast<vk::BaseOutStructure *>(
          reinterpret_cast<std::byte *>(el->pNext) + offset);
    }

    el = el->pNext;
  }
}

void detail::DynamicStructureChainBase::shrink_to_fit() {
  const void *old_data = data();
  dynamic_data.shrink_to_fit();
  fixup(old_data);
}

void detail::swap(DynamicStructureChainBase &a,
                  DynamicStructureChainBase &b) noexcept {
  const void *pa = a.data(), *pb = b.data();
  std::ranges::swap(a.dynamic_data, b.dynamic_data);
  a.fixup(pa);
  b.fixup(pb);
}