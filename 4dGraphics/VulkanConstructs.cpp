#include "VulkanConstructs.hpp"

#include "Device.hpp"
#include "v4dgVulkan.hpp"

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <memory>
#include <utility>

using namespace v4dg;
using namespace v4dg::detail;

BufferObject::BufferObject(internal_construct_t /*unused*/,
                           const Device &device,
                           std::pair<vma::Allocation, vk::raii::Buffer> p,
                           vk::DeviceSize size, bool hasDeviceAddress)
    : GpuAllocation(device.allocator(), p.first), m_buffer(std::move(p.second)),
      m_size(size) {
  if (hasDeviceAddress) {
    m_deviceAddress = device.device().getBufferAddress({buffer()});
  }
}

BufferObject::BufferObject(
    internal_construct_t /*unused*/, const Device &device,
    const vk::BufferCreateInfo &bufferCreateInfo,
    const vma::AllocationCreateInfo &allocationCreateInfo)
    : BufferObject(
          internal_construct_t{}, device,
          [&] {
            auto [buffer, allocation] = device.allocator().createBuffer(
                bufferCreateInfo, allocationCreateInfo);

            return std::pair<vma::Allocation, vk::raii::Buffer>(
                allocation, vk::raii::Buffer(device.device(), buffer));
          }(),
          bufferCreateInfo.size,
          static_cast<bool>(
              getBufferUsage(bufferCreateInfo) &
              vk::BufferUsageFlagBits2KHR::eShaderDeviceAddress)) {}

BufferObject::BufferObject(
    internal_construct_t /*unused*/, const Device &device, vk::DeviceSize size,
    vk::BufferUsageFlags2KHR usage,
    const vma::AllocationCreateInfo &allocationCreateInfo,
    vk::BufferCreateFlags flags,
    vk::ArrayProxy<const uint32_t> queueFamilyIndices)
    : BufferObject(
          internal_construct_t{}, device,
          vk::StructureChain<vk::BufferCreateInfo,
                             vk::BufferUsageFlags2CreateInfoKHR>{
              {
                  flags,
                  size,
                  {},
                  queueFamilyIndices.empty() ? vk::SharingMode::eExclusive
                                             : vk::SharingMode::eConcurrent,
                  queueFamilyIndices,
              },
              {usage},
          }
              .get<>(),
          allocationCreateInfo) {}

ImageObject::ImageObject(internal_construct_t /*unused*/, const Device &device,
                         std::pair<vma::Allocation, vk::raii::Image> image,
                         vk::ImageType imageType, vk::Format format,
                         vk::Extent3D extent, std::uint32_t mipLevels,
                         std::uint32_t arrayLayers,
                         vk::SampleCountFlagBits samples)
    : GpuAllocation(device.allocator(), image.first),
      m_image(std::move(image.second)), m_imageType(imageType),
      m_format(format), m_extent(extent), m_mipLevels(mipLevels),
      m_arrayLayers(arrayLayers), m_samples(samples) {}

ImageObject::ImageObject(internal_construct_t /*unused*/, const Device &device,
                         const ImageCreateInfo &imageCreateInfo,
                         const vma::AllocationCreateInfo &allocationCreateInfo)
    : ImageObject(
          internal_construct_t{}, device,
          [&] {
            vk::StructureChain<vk::ImageCreateInfo,
                               vk::ImageFormatListCreateInfo,
                               vk::ImageStencilUsageCreateInfo>
                ici{{
                        imageCreateInfo.flags,
                        imageCreateInfo.imageType,
                        imageCreateInfo.format,
                        imageCreateInfo.extent,
                        imageCreateInfo.mipLevels,
                        imageCreateInfo.arrayLayers,
                        imageCreateInfo.samples,
                        imageCreateInfo.tiling,
                        imageCreateInfo.usage,
                        imageCreateInfo.sharingMode,
                        imageCreateInfo.queueFamilyIndices,
                        imageCreateInfo.initialLayout,
                    },
                    {},
                    {}};

            if (imageCreateInfo.formats) {
              ici.get<vk::ImageFormatListCreateInfo>().setViewFormats(
                  *imageCreateInfo.formats);
            } else {
              ici.unlink<vk::ImageFormatListCreateInfo>();
            }

            if (imageCreateInfo.stencilUsage) {
              ici.get<vk::ImageStencilUsageCreateInfo>().setStencilUsage(
                  *imageCreateInfo.stencilUsage);
            } else {
              ici.unlink<vk::ImageStencilUsageCreateInfo>();
            }

            auto [image, allocation] = device.allocator().createImage(
                ici.get<vk::ImageCreateInfo>(), allocationCreateInfo);

            return std::pair<vma::Allocation, vk::raii::Image>(
                allocation, vk::raii::Image(device.device(), image));
          }(),
          imageCreateInfo.imageType, imageCreateInfo.format,
          imageCreateInfo.extent, imageCreateInfo.mipLevels,
          imageCreateInfo.arrayLayers, imageCreateInfo.samples) {}

Buffer::Buffer(const Device &device,
               const vk::BufferCreateInfo &bufferCreateInfo,
               const vma::AllocationCreateInfo &allocationCreateInfo)
    : std::shared_ptr<const BufferObject>(std::make_shared<BufferObject>(
          BufferObject::internal_construct_t{}, device, bufferCreateInfo,
          allocationCreateInfo)) {}

Buffer::Buffer(const Device &device, vk::DeviceSize size,
               vk::BufferUsageFlags2KHR usage,
               const vma::AllocationCreateInfo &allocationCreateInfo,
               vk::BufferCreateFlags flags,
               vk::ArrayProxy<const uint32_t> queueFamilyIndices)
    : std::shared_ptr<const BufferObject>(std::make_shared<BufferObject>(
          BufferObject::internal_construct_t{}, device, size, usage,
          allocationCreateInfo, flags, queueFamilyIndices)) {}

Image::Image(const Device &device,
             const ImageObject::ImageCreateInfo &imageCreateInfo,
             const vma::AllocationCreateInfo &allocationCreateInfo)
    : std::shared_ptr<const ImageObject>(std::make_shared<ImageObject>(
          ImageObject::internal_construct_t{}, device, imageCreateInfo,
          allocationCreateInfo)) {}