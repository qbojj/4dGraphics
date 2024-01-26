#pragma once

#include "Device.hpp"
#include "v4dgCore.hpp"

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <span>

namespace v4dg {
namespace detail {

class GpuAllocation : public std::enable_shared_from_this<GpuAllocation> {
public:
  GpuAllocation(vma::Allocator allocator, vma::UniqueAllocation allocation);

  vma::Allocator allocator() const noexcept { return m_allocator; }
  vma::Allocation allocation() const noexcept { return *m_allocation; }

  class MemoryMapDeleter {
  public:
    void operator()(void *) const noexcept {
      m_allocator.unmapMemory(m_allocation);
    }

  private:
    MemoryMapDeleter(vma::Allocator allocator,
                     vma::Allocation allocation) noexcept
        : m_allocator(allocator), m_allocation(allocation) {}
    vma::Allocator m_allocator;
    vma::Allocation m_allocation;

    friend GpuAllocation;
  };

  template <typename T> using MemoryMap = std::unique_ptr<T, MemoryMapDeleter>;

  template <typename T> MemoryMap<T> map() const {
    return MemoryMap<T>(static_cast<typename
MemoryMap<T>::pointer>(map_unsafe()), MemoryMapDeleter(allocator(),
allocation()));
  }

  void *map_unsafe() const { return allocator().mapMemory(allocation()); }
  void unmap_unsafe() const noexcept { allocator().unmapMemory(allocation()); }

  void flush(vk::DeviceSize offset = 0,
             vk::DeviceSize size = vk::WholeSize) const {
    allocator().flushAllocation(allocation(), offset, size);
  }
  void invalidate(vk::DeviceSize offset = 0,
                  vk::DeviceSize size = vk::WholeSize) const {
    allocator().invalidateAllocation(allocation(), offset, size);
  }

private:
  vma::Allocator m_allocator;
  vma::UniqueAllocation m_allocation;
};
} // namespace detail

class Buffer : public detail::GpuAllocation {
public:
  struct BufferCreateInfo {
    vk::BufferCreateFlags flags = {};
    vk::DeviceSize size = {};
    vk::BufferUsageFlags usage = {};
    vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
    std::vector<uint32_t> queueFamilyIndices = {};
    std::shared_ptr<const void> pNext = nullptr;
  };

  static Handle<Buffer>
  create(Handle<Device> device, BufferCreateInfo bufferCreateInfo,
         const vma::AllocationCreateInfo &allocationCreateInfo);

  vk::Buffer buffer() const { return *m_buffer; }
  operator vk::Buffer() const { return buffer(); }
  vk::Buffer operator*() const { return buffer(); }

  vk::DeviceAddress deviceAddress() const { return m_deviceAddress; }

private:
  vma::UniqueBuffer m_buffer;
  vk::DeviceAddress m_deviceAddress;

  Buffer(std::pair<vma::UniqueAllocation, vma::UniqueBuffer> buffer,
         vma::Allocator allocator, BufferCreateInfo bufferCreateInfo);

protected:
  struct Private {
    Handle<Device> device;
    BufferCreateInfo bufferCreateInfo;
    const vma::AllocationCreateInfo &allocationCreateInfo;
  };

public:
  Buffer(Private &&);
};

class Image : public detail::GpuAllocation {
public:
  struct ImageCreateInfo {
    vk::ImageCreateFlags flags = {};
    vk::ImageType imageType = vk::ImageType::e2D;
    vk::Format format = vk::Format::eUndefined;
    vk::Extent3D extent = {};
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    vk::ImageUsageFlags usage = {};
    vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
    std::vector<uint32_t> queueFamilyIndices = {};
    vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined;

    std::vector<vk::Format> viewFormatList = {};

    std::shared_ptr<const void> pNext = nullptr;
  };

  static Handle<Image>
  create(Handle<Device> device, ImageCreateInfo imageCreateInfo,
         const vma::AllocationCreateInfo &allocationCreateInfo);

  vk::Image image() const { return *m_image; }
  operator vk::Image() const { return image(); }
  vk::Image operator*() const { return image(); }

private:
  vma::UniqueImage m_image;
  ImageCreateInfo m_imageCreateInfo;

  Image(std::pair<vma::UniqueAllocation, vma::UniqueImage> image,
        Handle<Device> device, ImageCreateInfo imageCreateInfo);

protected:
  struct Private {
    Handle<Device> device;
    ImageCreateInfo imageCreateInfo;
    const vma::AllocationCreateInfo &allocationCreateInfo;
  };

public:
  Image(Private &&);
};

class Texture : public Image {
public:
  struct ImageViewCreateInfo {
    vk::ImageViewCreateFlags flags = {};
    vk::ImageViewType viewType = vk::ImageViewType::e2D;
    vk::Format format = vk::Format::eUndefined;
    vk::ComponentMapping components = {};
    vk::ImageSubresourceRange subresourceRange = {};
    std::shared_ptr<const void> pNext = nullptr;
  };

  static Handle<Texture>
  create(Handle<Device> device, ImageCreateInfo imageCreateInfo,
         const vma::AllocationCreateInfo &allocationCreateInfo,
         std::vector<ImageViewCreateInfo> imageViewCreateInfo);

private:
  std::vector<vk::UniqueImageView> m_imageViews;
  std::vector<ImageViewCreateInfo> m_imageViewCreateInfos;

  static std::vector<vk::UniqueImageView>
      makeImageViews(Handle<Device>, vk::Image,
                     std::span<const ImageViewCreateInfo>);

protected:
  struct Private {
    Image::Private imagePrivate;
    Handle<Device> device;
    std::vector<ImageViewCreateInfo> imageViewCreateInfos;
  };

public:
  Texture(Private &&);
};
/*
VkResult CreateTextureImage(VulkanThreadCtx &vkCtx, const char *filename,
                            VulkanTexture *texture);


VkResult CreateSSBOVertexBuffer( VulkanRenderDevice &vkDev,
        const char *filename,
        VkBuffer *storageBuffer, VmaAllocation *storageBufferMemory,
        VulkanBufferSuballocation *vertexBuffer,
        VulkanBufferSuballocation *indexBuffer
);
*/
} // namespace v4dg