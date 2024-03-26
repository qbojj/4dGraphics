#include "VulkanResources.hpp"

using namespace v4dg;

namespace {
bool hasAllFlags(auto flags, auto mask) { return (flags & mask) == mask; }

} // namespace

ImageView::ImageView(Context &ctx, vk::ImageViewCreateFlags flags,
                     vk::Image image, vk::ImageViewType viewType,
                     vk::Format format, vk::ImageUsageFlags usage,
                     vk::ComponentMapping components,
                     vk::ImageSubresourceRange subresourceRange)
    : m_imageView(nullptr)
    , m_viewType(viewType) {
  vk::StructureChain<vk::ImageViewCreateInfo, vk::ImageViewUsageCreateInfo>
      chain{{flags, image, viewType, format, components, subresourceRange},
            {usage}};

  if (!usage)
    chain.unlink<vk::ImageViewUsageCreateInfo>();

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

  if (cnt > 0)
    ctx.vkDevice().updateDescriptorSets(std::span{writes.data(), cnt}, {});
}

Texture::Texture(Context &ctx, const ImageCreateInfo &imageCreateInfo,
                 const vma::AllocationCreateInfo &allocationCreateInfo,
                 vk::ImageViewCreateFlags viewFlags, vk::ImageViewType viewType,
                 vk::ImageAspectFlags aspectFlags,
                 vk::ComponentMapping components)
    : Image(ctx.device(), imageCreateInfo, allocationCreateInfo),
      ImageView(ctx, viewFlags, image(), viewType, format(),
                imageCreateInfo.usage, components,
                {aspectFlags, 0, mipLevels(), 0, arrayLayers()}) {}