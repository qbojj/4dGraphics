#pragma once

#include "Device.hpp"
#include "v4dgCore.hpp"

#include <vulkan/vulkan.hpp>

#include <bit>
#include <cassert>
#include <ranges>
#include <mutex>
#include <deque>
#include <memory>

namespace v4dg {
enum class BindlessType : uint8_t {
  eSampler,
  eSampledImage,
  eStorageImage,
  eAccelerationStructureKHR,
};

class BindlessResource {
public:
  bool valid() const {
    return m_resource != 0;
  };

  uint32_t index() const { return (m_resource >> index_shift) & index_mask; }
  BindlessType type() const {
    return static_cast<BindlessType>((m_resource >> type_shift) & type_mask);
  }
  uint8_t version() const {
    return static_cast<uint8_t>((m_resource >> version_shift) & version_mask);
  }

  static vk::DescriptorType type_to_vk(BindlessType) noexcept;

  auto operator<=>(const BindlessResource &) const = default;
  explicit operator bool() const { return valid(); }
  bool operator!() const { return !valid(); }

private:
  static constexpr uint32_t index_mask = (1 << 23) - 1;
  static constexpr uint32_t index_shift = 0;

  static constexpr uint32_t type_mask = (1 << 2) - 1;
  static constexpr uint32_t type_shift = 23;

  static constexpr uint32_t version_mask = (1 << 6) - 1;
  static constexpr uint32_t version_shift = 25;

  BindlessResource(uint32_t index, BindlessType type, uint8_t version = 0) {
    uint32_t type_v = static_cast<uint32_t>(type);
    uint32_t version_v = static_cast<uint32_t>(version);

    assert(index <= index_mask);
    assert(type_v <= type_mask);
    assert(version_v <= version_mask);

    m_resource = (index << index_shift) | (type_v << type_shift) | (version_v << version_shift) | (1<<31);
  }

  void bump_version() {
    uint32_t new_version = (version() + 1) & version_mask;
    *this = BindlessResource(index(), type(), new_version);
  }

  static constexpr uint32_t max_count = index_mask + 1;

  friend class BindlessManager;

  uint32_t m_resource{0};
};

class BindlessManager {
public:
  using UniqueResource = std::unique_ptr<BindlessResource, class UniqueResourceDeleter>;

  BindlessManager(Handle<Device> device);

  const auto &get_layouts() const { return m_layouts; }

  void bind(const vk::raii::CommandBuffer &cb,
            vk::PipelineBindPoint bind_point) const {
    cb.bindDescriptorSets(bind_point, *m_pipelineLayout, 0, m_sets, {});
  }

private:
  class BindlessHeap {
  public:
    BindlessHeap() = default;
    void setup(BindlessType type, uint32_t max_count);

    BindlessResource allocate();
    void free(BindlessResource);

  private:
    BindlessType m_type;

    uint32_t m_max_count;
    uint32_t m_count{0};

    std::mutex m_mutex;
    std::deque<BindlessResource> m_free;
  };

  class UniqueResourceDeleter {
  public:
    UniqueResourceDeleter(BindlessHeap *heap) : m_heap(heap) {}
    void operator()(BindlessResource res) const;

  private:
    BindlessHeap *m_heap;
  };

  Handle<Device> m_device;
  std::array<vk::raii::DescriptorSetLayout, 4> m_layouts;
  vk::raii::PipelineLayout m_pipelineLayout;

  vk::raii::DescriptorPool m_pool;
  std::array<vk::DescriptorSet, 4> m_sets;
  std::array<BindlessHeap, 4> m_heaps;

  static std::array<uint32_t, 4> calculate_sizes(Device &device);
};
} // namespace v4dg