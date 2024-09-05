#include "VulkanResources.hpp"

#include "BindlessManager.hpp"
#include "Context.hpp"
#include "VulkanConstructs.hpp"

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <span>

using namespace v4dg;
using namespace v4dg::detail;

namespace {
bool hasAllFlags(auto flags, auto mask) { return (flags & mask) == mask; }
} // namespace

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
