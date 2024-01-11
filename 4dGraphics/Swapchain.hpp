#pragma once

#include "Device.hpp"
#include "v4dgCore.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>

namespace v4dg {
class Swapchain {
public:
  Swapchain(Handle<Device> device, vk::SurfaceKHR surface);

  const vk::raii::SwapchainKHR &swapchain() const { return m_swapchain; }

  const auto &images() const { return m_images; }

private:
  Handle<Device> m_device;

  vk::raii::SwapchainKHR m_swapchain;
  std::vector<vk::Image> m_images;
  std::vector<vk::raii::ImageView> m_imageViews;
  
};
} // namespace v4dg