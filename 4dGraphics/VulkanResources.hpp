#pragma once

#include "BindlessManager.hpp"
#include "Context.hpp"
#include "Device.hpp"
#include "VulkanConstructs.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <type_traits>

namespace v4dg {

/*
from
https://github.com/KhronosGroup/Vulkan-Docs/issues/2311#issuecomment-1959896807:
> With sync2, there are pretty much just 2 layouts a subresource 
>   accessible by shader should ever be in: GENERAL and READ_ONLY_OPTIMAL [...].
> Images that is only ever imageStored into and then sampled from,
>   you can just keep in GENERAL at likely no performance loss
>   (READ_ONLY helps when you're transitioning away from ATTACHMENT_OPTIMAL, on
>   some hardware.)
*/
class ImageView {
public:
  ImageView(Context &ctx, vk::ImageViewCreateFlags flags, vk::Image image,
            vk::ImageViewType viewType, vk::Format format,
            vk::ImageUsageFlags usage, vk::ComponentMapping components = {},
            vk::ImageSubresourceRange subresourceRange = {
                vk::ImageAspectFlagBits::eColor, 0, vk::RemainingMipLevels, 0,
                vk::RemainingArrayLayers});

  template <typename... Args>
  void setName(const Device &dev, std::format_string<Args...> fmt,
               Args &&...args) {
    dev.setDebugName(imageView(), fmt, std::forward<Args>(args)...);
  }

  vk::ImageView imageView() const { return *m_imageView; }
  vk::ImageViewType viewType() const { return m_viewType; }
  
  BindlessResource sampledOptimalHandle() const {
    return m_sampledOptimalHandle.get();
  }
  BindlessResource sampledGeneralHandle() const {
    return m_sampledGeneralHandle.get();
  }
  BindlessResource storageHandle() const { return m_storageHandle.get(); }

private:
  vk::raii::ImageView m_imageView;

  vk::ImageViewType m_viewType;

  // sampled handles
  UniqueBindlessResource m_sampledOptimalHandle;
  UniqueBindlessResource m_sampledGeneralHandle;

  // storage handle
  UniqueBindlessResource m_storageHandle;
};

/*
image with a single view
*/
class Texture : public Image, public ImageView {
public:
  Texture(Context &ctx, const ImageCreateInfo &imageCreateInfo,
          const vma::AllocationCreateInfo &allocationCreateInfo,
          vk::ImageViewCreateFlags flags = {},
          vk::ImageViewType viewType = vk::ImageViewType::e2D,
          vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor,
          vk::ComponentMapping components = {});

  template <typename... Args>
  void setName(const Device &dev, std::format_string<Args...> fmt,
               Args &&...args) {
    if (dev.debugNamesAvaiable()) {
      std::string name = std::format(fmt, std::forward<Args>(args)...);
      Image::setName(dev, "{}", name);
      ImageView::setName(dev, "{} view", name);
    }
  }
};
} // namespace v4dg