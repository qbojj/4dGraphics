#pragma once

#include "Device.hpp"
#include "v4dgCore.hpp"

#include <vulkan/vulkan.hpp>

#include <bit>
#include <cassert>
#include <deque>
#include <memory>
#include <mutex>
#include <ranges>
#include <utility>

namespace v4dg {
enum class BindlessType : uint8_t {
  eSampler,
  eSampledImage,
  eStorageImage,
  eAccelerationStructureKHR,
};

class BindlessManager;
class BindlessResource {
public:
  BindlessResource() noexcept = default;

  bool valid() const { return m_resource != 0; };

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

    m_resource = (index << index_shift) | (type_v << type_shift) |
                 (version_v << version_shift) | (1 << 31);
  }

  void bump_version() {
    uint32_t new_version = (version() + 1) & version_mask;
    *this = BindlessResource(index(), type(), new_version);
  }

  static constexpr uint32_t max_count = index_mask + 1;

  friend class BindlessManager;

  uint32_t m_resource{0};
};

class UniqueBindlessResource {
public:
  UniqueBindlessResource() noexcept = default;
  UniqueBindlessResource(BindlessResource res,
                         BindlessManager &manager) noexcept
      : m_res(res), m_manager(&manager) {}
  UniqueBindlessResource(const UniqueBindlessResource &) = delete;
  UniqueBindlessResource(UniqueBindlessResource &&o) noexcept
      : m_res(std::exchange(o.m_res, {})),
        m_manager(std::exchange(o.m_manager, nullptr)) {}
  UniqueBindlessResource &operator=(UniqueBindlessResource o) noexcept {
    std::swap(m_res, o.m_res);
    std::swap(m_manager, o.m_manager);
    return *this;
  }
  ~UniqueBindlessResource();

  auto operator<=>(const UniqueBindlessResource &) const = default;
  explicit operator bool() const { return static_cast<bool>(m_res); }
  bool operator!() const { return !m_res; }

  const BindlessResource &get() const { return m_res; }
  const BindlessResource &operator*() const { return get(); }
  BindlessResource release() { return std::exchange(m_res, {}); }

private:
  BindlessResource m_res;
  BindlessManager *m_manager{nullptr};
};

class BindlessManager {
public:
  static constexpr uint32_t layout_count = 4;
  static constexpr uint32_t resource_count = 4;

  BindlessManager(const Device &device);

  const auto &get_layouts() const { return m_layouts; }

  void bind(const vk::raii::CommandBuffer &cb,
            vk::PipelineLayout pipelineLayout,
            vk::PipelineBindPoint bind_point) const {
    cb.bindDescriptorSets(bind_point, pipelineLayout, 0, m_sets, {});
  }

  UniqueBindlessResource allocate(BindlessType type);
  void free(BindlessResource res);

  vk::WriteDescriptorSet write_for(BindlessResource res) const;
  vk::WriteDescriptorSet
  write_for(BindlessResource res,
            vk::ArrayProxyNoTemporaries<const vk::DescriptorImageInfo>
                image_info) const {
    return write_for(res).setImageInfo(image_info);
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

  const Device *m_device;
  std::array<vk::raii::DescriptorSetLayout, 4> m_layouts;

  vk::raii::DescriptorPool m_pool;
  std::array<vk::DescriptorSet, layout_count> m_sets;
  std::array<BindlessHeap, resource_count> m_heaps;

  static std::array<uint32_t, resource_count>
  calculate_sizes(const Device &device);
};
} // namespace v4dg