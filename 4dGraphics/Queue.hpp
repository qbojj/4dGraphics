#pragma once

#include <mutex>
#include <taskflow/taskflow.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <memory>
#include <thread>

namespace v4dg {
class Queue {
public:
  Queue(vk::raii::Queue queue, std::uint32_t family, std::uint32_t index,
        vk::QueueFlags flags, std::uint32_t timestampValidBits,
        vk::Extent3D minImageTransferGranularity)
      : m_queue(std::move(queue)), m_family(family), m_index(index),
        m_flags(flags), m_timestampValidBits(timestampValidBits),
        m_minImageTransferGranularity(minImageTransferGranularity) {}

  [[nodiscard]] auto &queue() const { return m_queue; }
  [[nodiscard]] std::uint32_t family() const { return m_family; }
  [[nodiscard]] std::uint32_t index() const { return m_index; }
  [[nodiscard]] vk::QueueFlags flags() const { return m_flags; }
  [[nodiscard]] std::uint32_t timestampValidBits() const {
    return m_timestampValidBits;
  }
  [[nodiscard]] vk::Extent3D minImageTransferGranularity() const {
    return m_minImageTransferGranularity;
  }

private:
  vk::raii::Queue m_queue;

  std::uint32_t m_family, m_index;

  vk::QueueFlags m_flags;
  std::uint32_t m_timestampValidBits;
  vk::Extent3D m_minImageTransferGranularity;
};
} // namespace v4dg