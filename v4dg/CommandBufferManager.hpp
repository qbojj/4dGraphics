#pragma once

#include "v4dgVulkan.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace v4dg {
class command_buffer_manager {
public:
  enum class category { c0_100, c100_500, c500_inf, cCount };

  command_buffer_manager(const vk::raii::Device &device, uint32_t family,
                         vk::CommandPoolCreateFlags flags =
                             vk::CommandPoolCreateFlagBits::eTransient);

  void reset(vk::CommandPoolResetFlags flags = {});
  vulkan_raii_view<vk::raii::CommandBuffer>
  get(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary,
      category cat = category::c0_100);

private:
  struct cache_bucket {
    std::vector<vk::CommandBuffer> buffers;
    std::size_t used{0};
  };

  static constexpr uint32_t block_count = 8;

  const vk::raii::Device *m_device;
  vk::raii::CommandPool m_pool;
  std::array<cache_bucket, (std::size_t)category::cCount> m_primary{},
      m_secondary{};
};
} // namespace v4dg
