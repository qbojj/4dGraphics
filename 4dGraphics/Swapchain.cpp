#include "Swapchain.hpp"

#include "Context.hpp"
#include "v4dgCore.hpp"

#include <functional>
#include <ranges>

namespace v4dg {
Swapchain SwapchainBuilder::build(Context &ctx) const {
  auto &pd = ctx.vkPhysicalDevice();

  auto surfaceCapabilities = pd.getSurfaceCapabilitiesKHR(surface);
  auto surfaceFormats =
      pd.getSurfaceFormatsKHR(surface) |
      std::views::filter([](vk::SurfaceFormatKHR sf) {
        return sf.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      }) |
      std::views::transform([](vk::SurfaceFormatKHR sf) { return sf.format; });

  auto presentModes = pd.getSurfacePresentModesKHR(surface);

  if (std::ranges::empty(surfaceFormats))
    throw exception("No SRGB surface formats available");

  vk::Extent2D extent = this->extent;

  if (extent.width == 0 || extent.height == 0) {
    if (surfaceCapabilities.currentExtent.width == 0xffffffff) {
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

  if (extent.width == 0 || extent.height == 0)
    throw exception("Invalid swapchain extent");

  vk::Format format;

  auto size_one = [](auto &&range) { return ++range.begin() == range.end(); };
  auto first_element = [](auto &&range) { return *range.begin(); };

  if (size_one(surfaceFormats) &&
      first_element(surfaceFormats) == vk::Format::eUndefined) {
    // ok to use any format
    format = preferred_format;
  } else {
    if (required_format) {
      if (std::ranges::contains(surfaceFormats, *required_format)) {
        format = *required_format;
      } else
        throw exception("Required surface format {} not supported",
                        *required_format);
    } else {
      if (std::ranges::contains(surfaceFormats, preferred_format)) {
        format = preferred_format;
      } else {
        if (std::ranges::contains(surfaceFormats, vk::Format::eB8G8R8A8Unorm))
          format = vk::Format::eB8G8R8A8Unorm;
        else if (std::ranges::contains(surfaceFormats,
                                       vk::Format::eR8G8B8A8Unorm))
          format = vk::Format::eR8G8B8A8Unorm;
        else {
          // every implementation I know of supports either BGRA8 or RGBA8
          //  so this should never happen
          format = *surfaceFormats.begin();
        }
      }
    }
  }

  vk::PresentModeKHR presentMode;

  if (required_present_mode) {
    if (std::ranges::contains(presentModes, *required_present_mode)) {
      presentMode = *required_present_mode;
    } else
      throw exception("Required present mode {} not supported",
                      *required_present_mode);
  } else {
    if (std::ranges::contains(presentModes, preferred_present_mode)) {
      presentMode = preferred_present_mode;
    } else {
      presentMode = vk::PresentModeKHR::eFifo;
    }
  }

  vk::SurfaceTransformFlagBitsKHR preTransform = pre_transform;
  vk::CompositeAlphaFlagBitsKHR compositeAlpha = composite_alpha;

  if (!(surfaceCapabilities.supportedTransforms & preTransform))
    preTransform = surfaceCapabilities.currentTransform;

  if (!(surfaceCapabilities.supportedCompositeAlpha & compositeAlpha))
    compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

  uint32_t imageCount =
      std::max(image_count, surfaceCapabilities.minImageCount);
  if (surfaceCapabilities.maxImageCount != 0)
    imageCount = std::min(imageCount, surfaceCapabilities.maxImageCount);

  return Swapchain(ctx,
                   vk::SwapchainCreateInfoKHR(
                       {}, surface, imageCount, format,
                       vk::ColorSpaceKHR::eSrgbNonlinear, extent, 1, imageUsage,
                       vk::SharingMode::eExclusive, 0, nullptr, preTransform,
                       compositeAlpha, presentMode, true, oldSwapchain));
}

Swapchain::Swapchain(Context &ctx, const vk::SwapchainCreateInfoKHR &ci)
    : m_format(ci.imageFormat), m_colorSpace(ci.imageColorSpace),
      m_presentMode(ci.presentMode), m_extent(ci.imageExtent),
      m_preTransform(ci.preTransform), m_compositeAlpha(ci.compositeAlpha),
      m_imageUsage(ci.imageUsage), m_swapchain(ctx.vkDevice(), ci),
      m_images(m_swapchain.getImages()) {
  m_imageViews.reserve(m_images.size());
  for (auto &image : m_images)
    m_imageViews.push_back({ctx.vkDevice(),
                            {{},
                             image,
                             vk::ImageViewType::e2D,
                             m_format,
                             {},
                             {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}});

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