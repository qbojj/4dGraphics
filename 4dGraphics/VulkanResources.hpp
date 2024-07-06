#pragma once

#include "BindlessManager.hpp"
#include "Context.hpp"
#include "Device.hpp"
#include "VulkanConstructs.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <type_traits>

namespace v4dg {
class ImageView;
namespace detail {

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
class ImageViewObject {
private:
  struct internal_construct_t {};

public:
  template <typename... Args>
  void setName(const Device &dev, std::format_string<Args...> fmt,
               Args &&...args) const {
    dev.setDebugName(imageView(), fmt, std::forward<Args>(args)...);
  }

  [[nodiscard]] auto image() const noexcept { return m_image; }
  [[nodiscard]] auto vkImage() const noexcept { return image()->vk(); }

  [[nodiscard]] vk::ImageView imageView() const noexcept {
    return *m_imageView;
  }
  [[nodiscard]] vk::ImageView vk() const noexcept { return imageView(); }

  [[nodiscard]] vk::ImageViewType viewType() const noexcept {
    return m_viewType;
  }

  [[nodiscard]] BindlessResource sampledOptimalHandle() const noexcept {
    return m_sampledOptimalHandle.get();
  }
  [[nodiscard]] BindlessResource sampledGeneralHandle() const noexcept {
    return m_sampledGeneralHandle.get();
  }
  [[nodiscard]] BindlessResource storageHandle() const noexcept {
    return m_storageHandle.get();
  }

  ImageViewObject(internal_construct_t, Context &ctx, Image image,
                  vk::ImageViewCreateFlags flags, vk::ImageViewType viewType,
                  vk::Format format, vk::ImageUsageFlags usage,
                  vk::ComponentMapping components,
                  vk::ImageSubresourceRange subresourceRange);

private:
  Image m_image;
  vk::raii::ImageView m_imageView;

  vk::ImageViewType m_viewType;

  // sampled handles
  UniqueBindlessResource m_sampledOptimalHandle;
  UniqueBindlessResource m_sampledGeneralHandle;

  // storage handle
  UniqueBindlessResource m_storageHandle;

  friend ImageView;
};
} // namespace detail

class ImageView : public std::shared_ptr<const detail::ImageViewObject> {
public:
  ImageView() = delete;

  ImageView(Context &ctx, Image image, vk::ImageViewCreateFlags flags,
            vk::ImageViewType viewType, vk::Format format,
            vk::ImageUsageFlags usage, vk::ComponentMapping components = {},
            vk::ImageSubresourceRange subresourceRange = {
                vk::ImageAspectFlagBits::eColor, 0, vk::RemainingMipLevels, 0,
                vk::RemainingArrayLayers});

  // create a view that owns image (texture)
  [[nodiscard]] static ImageView createTexture(
      Context &ctx, const Image::ImageCreateInfo &imageCreateInfo,
      const vma::AllocationCreateInfo &allocationCreateInfo =
          {{}, vma::MemoryUsage::eAuto},
      vk::ImageViewCreateFlags flags = {},
      vk::ImageViewType viewType = vk::ImageViewType::e2D,
      vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor,
      vk::ComponentMapping components = {});
};

} // namespace v4dg