module;

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <span>

export module v4dg:resources;

import :constructs;
import :device;
import :context;
import :bindlessManager;

import v4dg.cppHelpers;

import vulkan_hpp;

namespace v4dg {
export class ImageView;
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

  ImageViewObject(internal_construct_t, Context &ctx, const Image &image,
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

export class ImageView : public std::shared_ptr<const detail::ImageViewObject> {
public:
  ImageView() = delete;

  ImageView(Context &ctx, const Image &image, vk::ImageViewCreateFlags flags,
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

ImageViewObject::ImageViewObject(internal_construct_t /*unused*/, Context &ctx,
                                 const Image &image,
                                 vk::ImageViewCreateFlags flags,
                                 vk::ImageViewType viewType, vk::Format format,
                                 vk::ImageUsageFlags usage,
                                 vk::ComponentMapping components,
                                 vk::ImageSubresourceRange subresourceRange)
    : m_image(image), m_imageView(nullptr), m_viewType(viewType) {
  vk::StructureChain<vk::ImageViewCreateInfo, vk::ImageViewUsageCreateInfo>
      chain{{flags, image->image(), viewType, format, components,
             subresourceRange},
            {usage}};

  if (!usage) {
    chain.unlink<vk::ImageViewUsageCreateInfo>();
  }

  m_imageView = {ctx.vkDevice(), chain.get<>()};

  std::array<vk::WriteDescriptorSet, 3> writes;
  std::size_t cnt{0};

  vk::DescriptorImageInfo image_general_info{
      {}, *m_imageView, vk::ImageLayout::eGeneral};

  vk::DescriptorImageInfo image_optimal_info{
      {}, *m_imageView, vk::ImageLayout::eReadOnlyOptimal};

  if (hasAllFlags(usage, vk::ImageUsageFlagBits::eSampled)) {
    m_sampledOptimalHandle =
        ctx.bindlessManager().allocate(BindlessType::eSampledImage);
    writes[cnt++] = ctx.bindlessManager().write_for(*m_sampledOptimalHandle,
                                                    image_optimal_info);
  }

  if (hasAllFlags(usage, vk::ImageUsageFlagBits::eSampled |
                             vk::ImageUsageFlagBits::eStorage)) {
    m_sampledGeneralHandle =
        ctx.bindlessManager().allocate(BindlessType::eSampledImage);
    writes[cnt++] = ctx.bindlessManager().write_for(*m_sampledGeneralHandle,
                                                    image_general_info);
  }

  if (hasAllFlags(usage, vk::ImageUsageFlagBits::eStorage)) {
    m_storageHandle =
        ctx.bindlessManager().allocate(BindlessType::eStorageImage);
    writes[cnt++] =
        ctx.bindlessManager().write_for(*m_storageHandle, image_general_info);
  }

  if (cnt > 0) {
    ctx.vkDevice().updateDescriptorSets(std::span{writes.data(), cnt}, {});
  }
}

ImageView::ImageView(Context &ctx, const Image &image,
                     vk::ImageViewCreateFlags flags, vk::ImageViewType viewType,
                     vk::Format format, vk::ImageUsageFlags usage,
                     vk::ComponentMapping components,
                     vk::ImageSubresourceRange subresourceRange)
    : std::shared_ptr<const ImageViewObject>(std::make_shared<ImageViewObject>(
          ImageViewObject::internal_construct_t{}, ctx, image, flags, viewType,
          format, usage, components, subresourceRange)) {}

ImageView ImageView::createTexture(
    Context &ctx, const Image::ImageCreateInfo &imageCreateInfo,
    const vma::AllocationCreateInfo &allocationCreateInfo,
    vk::ImageViewCreateFlags viewFlags, vk::ImageViewType viewType,
    vk::ImageAspectFlags aspectFlags, vk::ComponentMapping components) {
  return ImageView{
      ctx,
      Image{ctx.device(), imageCreateInfo, allocationCreateInfo},
      viewFlags,
      viewType,
      imageCreateInfo.format,
      imageCreateInfo.usage,
      components,
      {aspectFlags, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers},
  };
}

} // namespace v4dg
