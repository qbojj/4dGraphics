#pragma once

#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>
#include <optional>
#include <functional>

namespace v4dg {

class Context;
class Swapchain;

struct SwapchainBuilder {
  vk::SurfaceKHR surface = {};

  vk::Format preferred_format = vk::Format::eB8G8R8A8Unorm;
  std::optional<vk::Format> required_format = {};

  vk::PresentModeKHR preferred_present_mode = vk::PresentModeKHR::eFifoRelaxed;
  std::optional<vk::PresentModeKHR> required_present_mode = {};

  // if (0,0) then use the surface's currentExtent
  vk::Extent2D extent = {0, 0};
  vk::Extent2D fallback_extent = {1280, 720};

  vk::SurfaceTransformFlagBitsKHR pre_transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
  vk::CompositeAlphaFlagBitsKHR composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

  vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                                   vk::ImageUsageFlagBits::eTransferDst;

  vk::SwapchainKHR oldSwapchain = {};

  uint32_t image_count = 3;

  Swapchain build(Context &ctx) const;
};

class Swapchain {
public:
  const vk::raii::SwapchainKHR &swapchain() const { return m_swapchain; }

  const auto &images() const { return m_images; }
  const auto &imageViews() const { return m_imageViews; }

  vk::Format format() const { return m_format; }
  vk::ColorSpaceKHR colorSpace() const { return m_colorSpace; }
  vk::PresentModeKHR presentMode() const { return m_presentMode; }
  vk::Extent2D extent() const { return m_extent; }

  vk::SurfaceTransformFlagBitsKHR preTransform() const { return m_preTransform; }
  vk::CompositeAlphaFlagBitsKHR compositeAlpha() const { return m_compositeAlpha; }

  vk::ImageUsageFlags getImageUsage() const { return m_imageUsage; }

  DestructionItem move_out();

  SwapchainBuilder recreate_builder() const;

private:
  friend SwapchainBuilder;

  Swapchain(Context &, const vk::SwapchainCreateInfoKHR &);

  vk::Format m_format;
  vk::ColorSpaceKHR m_colorSpace;
  vk::PresentModeKHR m_presentMode;
  vk::Extent2D m_extent;

  vk::SurfaceTransformFlagBitsKHR m_preTransform;
  vk::CompositeAlphaFlagBitsKHR m_compositeAlpha;

  vk::ImageUsageFlags m_imageUsage;

  vk::raii::SwapchainKHR m_swapchain;
  std::vector<vk::Image> m_images;
  std::vector<vk::raii::ImageView> m_imageViews;
};
} // namespace v4dg