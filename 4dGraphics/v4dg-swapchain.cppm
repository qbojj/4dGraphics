module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

export module v4dg:swapchain;

import :core;

import vulkan_hpp;

export namespace v4dg {

class Context;
class Swapchain;

struct SwapchainBuilder {
  vk::SurfaceKHR surface;

  std::optional<vk::Format> preferred_format = {};
  std::optional<vk::Format> required_format = {};

  vk::PresentModeKHR preferred_present_mode = vk::PresentModeKHR::eFifoRelaxed;
  std::optional<vk::PresentModeKHR> required_present_mode = {};

  // if (0,0) then use the surface's currentExtent
  vk::Extent2D extent = {0, 0};
  vk::Extent2D fallback_extent = {1280, 720};

  vk::SurfaceTransformFlagBitsKHR pre_transform =
      vk::SurfaceTransformFlagBitsKHR::eIdentity;
  vk::CompositeAlphaFlagBitsKHR composite_alpha =
      vk::CompositeAlphaFlagBitsKHR::eOpaque;

  vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                                   vk::ImageUsageFlagBits::eTransferDst;

  vk::SwapchainKHR oldSwapchain = {};

  uint32_t image_count = 3;

  Swapchain build(Context &ctx) const;
  vk::Format getFormat(Context &ctx) const;
  vk::PresentModeKHR getPresentMode(Context &ctx) const;
  vk::Extent2D
  getExtent(const vk::SurfaceCapabilitiesKHR &surfaceCapabilities) const;
};

class Swapchain {
public:
  [[nodiscard]] const vk::raii::SwapchainKHR &swapchain() const {
    return m_swapchain;
  }

  [[nodiscard]] const auto &images() const { return m_images; }
  [[nodiscard]] const vk::Image &image(size_t idx) const {
    return m_images[idx];
  }

  [[nodiscard]] const auto &imageViews() const { return m_imageViews; }
  [[nodiscard]] const vk::ImageView &imageView(size_t idx) const {
    return *m_imageViews[idx];
  }

  [[nodiscard]] const auto &readyToPresents() const { return m_readyToPresent; }
  [[nodiscard]] const vk::Semaphore &readyToPresent(size_t idx) const {
    return *m_readyToPresent[idx];
  }

  [[nodiscard]] vk::Format format() const { return m_format; }
  [[nodiscard]] vk::ColorSpaceKHR colorSpace() const { return m_colorSpace; }
  [[nodiscard]] vk::PresentModeKHR presentMode() const { return m_presentMode; }
  [[nodiscard]] vk::Extent2D extent() const { return m_extent; }

  [[nodiscard]] vk::SurfaceTransformFlagBitsKHR preTransform() const {
    return m_preTransform;
  }
  [[nodiscard]] vk::CompositeAlphaFlagBitsKHR compositeAlpha() const {
    return m_compositeAlpha;
  }

  [[nodiscard]] vk::ImageUsageFlags getImageUsage() const {
    return m_imageUsage;
  }

  DestructionItem move_out();

  [[nodiscard]] SwapchainBuilder recreate_builder() const;

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
  std::vector<vk::raii::Semaphore> m_readyToPresent;
};
} // namespace v4dg
