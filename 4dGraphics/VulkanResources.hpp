#pragma once

#include "Context.hpp"
#include "BindlessManager.hpp"
#include "Device.hpp"
#include "VulkanConstructs.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <type_traits>

namespace v4dg {

/*
image with a single view

from https://github.com/KhronosGroup/Vulkan-Docs/issues/2311#issuecomment-1959896807:
> Images that is only ever imageStored into and then sampled from,
>   you can just keep in GENERAL at likely no performance loss 
>   (READ_ONLY helps when you're transitioning away from ATTACHMENT_OPTIMAL, on some hardware.)
*/
class Texture : public Image {
public:
  Texture(Context &ctx, const ImageCreateInfo &imageCreateInfo,
          const vma::AllocationCreateInfo &allocationCreateInfo,
          vk::ImageViewCreateFlags flags = {},
          vk::ImageViewType viewType = vk::ImageViewType::e2D,
          vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor,
          vk::ComponentMapping components = {},
          const char *name = nullptr);

  vk::ImageView imageView() const { return *m_imageView; }
  BindlessResource sampledOptimalHandle() const { return m_sampledOptimalHandle.get(); }
  BindlessResource sampledGeneralHandle() const { return m_sampledGeneralHandle.get(); }
  BindlessResource storageHandle() const { return m_storageHandle.get(); }

  void setName(const Device &dev, std::string name) {
    Image::setName(dev, name.c_str());
    name.append(" view");
    dev.setDebugNameString(imageView(), name);
  }

  template <typename... Args>
  void setName(const Device &dev, std::format_string<Args...> fmt,
               Args &&...args) {
    if (dev.debugNamesAvaiable()) {
      setName(dev, std::format(fmt, std::forward<Args>(args)...));
    }
  }

private:
  vk::raii::ImageView m_imageView;

  // sampled handles
  UniqueBindlessResource m_sampledOptimalHandle;
  UniqueBindlessResource m_sampledGeneralHandle;

  // storage handle
  UniqueBindlessResource m_storageHandle;
};

template <typename T>
  requires std::is_trivially_copyable_v<T>
Buffer uploadBuffer(Context &ctx, std::span<const T> data,
                    vk::BufferUsageFlags2KHR usage,
                    vk::MemoryPropertyFlags memoryProperties =
                        vk::MemoryPropertyFlagBits::eDeviceLocal,
                    const char *name = nullptr) {
  return uploadBuffer(ctx, std::as_bytes(data), usage, memoryProperties, name);
}

Buffer uploadBuffer(Context &ctx, std::span<const std::byte> data,
                    vk::BufferUsageFlags2KHR usage,
                    vk::MemoryPropertyFlags memoryProperties =
                        vk::MemoryPropertyFlagBits::eDeviceLocal,
                    const char *name = nullptr);

} // namespace v4dg