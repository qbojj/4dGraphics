#include "Swapchain.hpp"

#include "Context.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <limits>
#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <utility>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace v4dg {
vk::Format SwapchainBuilder::getFormat(Context &ctx) const {
  const auto &pd = ctx.vkPhysicalDevice();

  auto surfaceFormats =
      pd.getSurfaceFormatsKHR(surface) |
      std::views::filter([](vk::SurfaceFormatKHR sf) {
        return sf.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      }) |
      std::views::transform([](vk::SurfaceFormatKHR sf) { return sf.format; });

  if (required_format) {
    if (std::ranges::contains(surfaceFormats, *required_format)) {
      return *required_format;
    }

    throw exception("Required surface format {} not supported",
                    *required_format);
  }

  if (preferred_format) {
    if (std::ranges::contains(surfaceFormats, *preferred_format)) {
      return *preferred_format;
    }
  }

  return surfaceFormats.front();
}

vk::PresentModeKHR SwapchainBuilder::getPresentMode(Context &ctx) const {
  const auto &pd = ctx.vkPhysicalDevice();

  auto presentModes = pd.getSurfacePresentModesKHR(surface);

  if (required_present_mode) {
    if (std::ranges::contains(presentModes, *required_present_mode)) {
      return *required_present_mode;
    }

    throw exception("Required present mode {} not supported",
                    *required_present_mode);
  }

  if (std::ranges::contains(presentModes, preferred_present_mode)) {
    return preferred_present_mode;
  }

  return vk::PresentModeKHR::eFifo;
}

vk::Extent2D SwapchainBuilder::getExtent(
    const vk::SurfaceCapabilitiesKHR &surfaceCapabilities) const {
  vk::Extent2D extent = this->extent;

  if (extent.width == 0 || extent.height == 0) {
    if (surfaceCapabilities.currentExtent.width ==
        std::numeric_limits<std::uint32_t>::max()) {
      extent = fallback_extent;
    } else {
      extent = surfaceCapabilities.currentExtent;
    }
  }

  extent.width =
      std::clamp(extent.width, surfaceCapabilities.minImageExtent.width,
                 surfaceCapabilities.maxImageExtent.width);
  extent.height =
      std::clamp(extent.height, surfaceCapabilities.minImageExtent.height,
                 surfaceCapabilities.maxImageExtent.height);

  if (extent.width == 0 || extent.height == 0) {
    throw exception("Invalid swapchain extent");
  }

  return extent;
}

Swapchain SwapchainBuilder::build(Context &ctx) const {
  const auto &pd = ctx.vkPhysicalDevice();
  auto surfaceCapabilities = pd.getSurfaceCapabilitiesKHR(surface);

  auto format = getFormat(ctx);
  auto presentMode = getPresentMode(ctx);
  auto extent = getExtent(surfaceCapabilities);

  vk::SurfaceTransformFlagBitsKHR preTransform = pre_transform;
  vk::CompositeAlphaFlagBitsKHR compositeAlpha = composite_alpha;

  if (!(surfaceCapabilities.supportedTransforms & preTransform)) {
    preTransform = surfaceCapabilities.currentTransform;
  }

  if (!(surfaceCapabilities.supportedCompositeAlpha & compositeAlpha)) {
    compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  }

  uint32_t imageCount =
      std::max(image_count, surfaceCapabilities.minImageCount);
  if (surfaceCapabilities.maxImageCount != 0) {
    imageCount = std::min(imageCount, surfaceCapabilities.maxImageCount);
  }

  return {
      ctx,
      {
          {},
          surface,
          imageCount,
          format,
          vk::ColorSpaceKHR::eSrgbNonlinear,
          extent,
          1,
          imageUsage,
          vk::SharingMode::eExclusive,
          0,
          nullptr,
          preTransform,
          compositeAlpha,
          presentMode,
          1U,
          oldSwapchain,
      },
  };
}

Swapchain::Swapchain(Context &ctx, const vk::SwapchainCreateInfoKHR &ci)
    : m_format(ci.imageFormat), m_colorSpace(ci.imageColorSpace),
      m_presentMode(ci.presentMode), m_extent(ci.imageExtent),
      m_preTransform(ci.preTransform), m_compositeAlpha(ci.compositeAlpha),
      m_imageUsage(ci.imageUsage), m_swapchain(ctx.vkDevice(), ci),
      m_images(m_swapchain.getImages()) {
  m_imageViews.reserve(m_images.size());
  for (auto &image : m_images) {
    m_imageViews.push_back({ctx.vkDevice(),
                            {{},
                             image,
                             vk::ImageViewType::e2D,
                             m_format,
                             {},
                             {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}});
  }

  m_readyToPresent.reserve(m_images.size());
  for (size_t i = 0; i < m_images.size(); ++i) {
    m_readyToPresent.push_back(ctx.vkDevice().createSemaphore({}));
  }
}

DestructionItem Swapchain::move_out() {
  return [views = std::move(m_imageViews), swp = std::move(m_swapchain),
          readyToPresent = std::move(m_readyToPresent)] noexcept {};
}

SwapchainBuilder Swapchain::recreate_builder() const {
  return SwapchainBuilder{
      .surface = {},
      .required_format = m_format,
      .preferred_present_mode = m_presentMode,
      .extent = m_extent,
      .pre_transform = m_preTransform,
      .composite_alpha = m_compositeAlpha,
      .imageUsage = m_imageUsage,
      .oldSwapchain = *m_swapchain,
      .image_count = uint32_t(m_images.size()),
  };
}
} // namespace v4dg
