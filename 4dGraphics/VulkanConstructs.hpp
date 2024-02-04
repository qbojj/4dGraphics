#pragma once

#include "Device.hpp"
#include "v4dgCore.hpp"

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace v4dg {
namespace detail {

class GpuAllocation {
private:
  struct map_deleter_type {
    void operator()(void *) const {
      obj->allocator().unmapMemory(obj->allocation());
    }
    GpuAllocation *obj;
  };

public:
  GpuAllocation(vma::Allocator allocator, vma::Allocation allocation)
      : m_allocator(allocator), m_allocation(allocation) {}
  
  ~GpuAllocation();

  GpuAllocation(const GpuAllocation &) = delete;
  GpuAllocation &operator=(const GpuAllocation &) = delete;

  GpuAllocation(GpuAllocation &&o) noexcept : m_allocator(o.m_allocator), m_allocation(std::exchange(o.m_allocation, {})) {}
  GpuAllocation &operator=(GpuAllocation o) noexcept {
    std::swap(m_allocator, o.m_allocator);
    std::swap(m_allocation, o.m_allocation);
    return *this;
  }

  vma::Allocator allocator() const noexcept { return m_allocator; }
  vma::Allocation allocation() const noexcept { return m_allocation; }

  template <typename T> using map_type = std::unique_ptr<T, map_deleter_type>;

  template <typename T> map_type<T> map() const {
    return make_unique_obj(
        static_cast<T *>(allocator().mapMemory(allocation())), {this});
  }

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
  vma::Allocation m_allocation;
};
} // namespace detail

class Buffer : public detail::GpuAllocation {
public:
  Buffer(const Device &dev,
          std::pair<vma::Allocation, vk::raii::Buffer> buffer);

  Buffer(const Device &dev, const vk::BufferCreateInfo &bufferCreateInfo,
         const vma::AllocationCreateInfo &allocationCreateInfo);

  vk::Buffer buffer() const { return *m_buffer; }
  operator vk::Buffer() const { return buffer(); }
  vk::Buffer operator*() const { return buffer(); }

  vk::DeviceAddress deviceAddress() const { return m_deviceAddress; }

private:
  vk::raii::Buffer m_buffer;
  vk::DeviceAddress m_deviceAddress;
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
    std::span<const uint32_t> queueFamilyIndices = {};
    vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined;

    // mutable formats
    std::optional<std::span<const vk::Format>> formats = {};

    std::optional<vk::ImageUsageFlags> stencilUsage = {};
  };

  Image(const Device &device,
        std::pair<vma::Allocation, vk::raii::Image> image,
        vk::ImageType imageType, vk::Format format, vk::Extent3D extent,
        uint32_t mipLevels, uint32_t arrayLayers,
        vk::SampleCountFlagBits samples);

  Image(const Device &device, const ImageCreateInfo &imageCreateInfo,
        const vma::AllocationCreateInfo &allocationCreateInfo);

  vk::Image image() const { return *m_image; }

  vk::ImageType imageType() const { return m_imageType; }
  vk::Format format() const { return m_format; }
  vk::Extent3D extent() const { return m_extent; }
  uint32_t mipLevels() const { return m_mipLevels; }
  uint32_t arrayLayers() const { return m_arrayLayers; }
  vk::SampleCountFlagBits samples() const { return m_samples; }

private:
  vk::raii::Image m_image;

  vk::ImageType m_imageType;
  vk::Format m_format;
  vk::Extent3D m_extent;
  uint32_t m_mipLevels;
  uint32_t m_arrayLayers;
  vk::SampleCountFlagBits m_samples;
};

// image with a single view
class Texture : public Image {
public:
  Texture(const Device &device, const ImageCreateInfo &imageCreateInfo,
          const vma::AllocationCreateInfo &allocationCreateInfo,
          vk::ImageViewCreateFlags flags = {},
          vk::ImageViewType viewType = vk::ImageViewType::e2D,
          vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor,
          vk::ComponentMapping components = {});

  vk::ImageView imageView() const { return *m_imageView; }

private:
  vk::raii::ImageView m_imageView;
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