#include "VulkanResources.hpp"

using namespace v4dg;

namespace {
bool hasAllFlags(auto flags, auto mask) { return (flags & mask) == mask; }
} // namespace

Texture::Texture(Context &ctx, const ImageCreateInfo &imageCreateInfo,
                 const vma::AllocationCreateInfo &allocationCreateInfo,
                 vk::ImageViewCreateFlags viewFlags, vk::ImageViewType viewType,
                 vk::ImageAspectFlags aspectFlags,
                 vk::ComponentMapping components, const char *name)
    : Image(ctx.device(), imageCreateInfo, allocationCreateInfo),
      m_imageView({ctx.vkDevice(),
                   {viewFlags,
                    image(),
                    viewType,
                    format(),
                    components,
                    {aspectFlags, 0, vk::RemainingMipLevels, 0,
                     vk::RemainingArrayLayers}}}),
      m_sampledOptimalHandle(
          hasAllFlags(imageCreateInfo.usage, vk::ImageUsageFlagBits::eSampled)
              ? ctx.bindlessManager().allocate(BindlessType::eSampledImage)
              : UniqueBindlessResource{}),
      m_sampledGeneralHandle(
          hasAllFlags(imageCreateInfo.usage,
                      vk::ImageUsageFlagBits::eSampled |
                          vk::ImageUsageFlagBits::eStorage)
              ? ctx.bindlessManager().allocate(BindlessType::eSampledImage)
              : UniqueBindlessResource{}),
      m_storageHandle(
          hasAllFlags(imageCreateInfo.usage, vk::ImageUsageFlagBits::eStorage)
              ? ctx.bindlessManager().allocate(BindlessType::eStorageImage)
              : UniqueBindlessResource{}) {
  std::array<vk::WriteDescriptorSet, 3> writes;
  std::size_t cnt{0};

  vk::DescriptorImageInfo image_general_info{
      {},
      *m_imageView,
      vk::ImageLayout::eGeneral,
  };

  vk::DescriptorImageInfo image_optimal_info{
      {},
      *m_imageView,
      vk::ImageLayout::eReadOnlyOptimal,
  };

  if (m_sampledOptimalHandle)
    writes[cnt++] = ctx.bindlessManager().write_for(*m_sampledOptimalHandle,
                                                    image_optimal_info);

  if (m_sampledGeneralHandle)
    writes[cnt++] = ctx.bindlessManager().write_for(*m_sampledGeneralHandle,
                                                    image_general_info);

  if (m_storageHandle)
    writes[cnt++] =
        ctx.bindlessManager().write_for(*m_storageHandle, image_general_info);

  if (cnt > 0)
    ctx.vkDevice().updateDescriptorSets(std::span{writes.data(), cnt}, {});

  if (name)
    setName(ctx.device(), "{}", name);
}
