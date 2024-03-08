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
    void operator()(void *) const { allocator.unmapMemory(allocation); }
    vma::Allocator allocator;
    vma::Allocation allocation;
  };

public:
  GpuAllocation(vma::Allocator allocator, vma::Allocation allocation)
      : m_allocator(allocator), m_allocation(allocation) {}

  GpuAllocation() = delete;
  GpuAllocation(const GpuAllocation &) = delete;
  GpuAllocation &operator=(const GpuAllocation &) = delete;
  GpuAllocation(GpuAllocation &&o) noexcept
      : m_allocator(o.m_allocator),
        m_allocation(std::exchange(o.m_allocation, {})) {}
  GpuAllocation &operator=(GpuAllocation &&o) noexcept {
    if (this != &o) {
      if (m_allocation)
        m_allocator.freeMemory(m_allocation);

      m_allocator = o.m_allocator;
      m_allocation = std::exchange(o.m_allocation, {});
    }
    return *this;
  }
  ~GpuAllocation() {
    if (m_allocation) {
      m_allocator.freeMemory(m_allocation);
    }
  }

  vma::Allocator allocator() const noexcept { return m_allocator; }
  vma::Allocation allocation() const noexcept { return m_allocation; }

  template <typename T> using map_type = std::unique_ptr<T[], map_deleter_type>;

  template <typename T> map_type<T> map() {
    return map_type<T>(static_cast<T *>(allocator().mapMemory(allocation())),
                       {allocator(), allocation()});
  }

  void flush(vk::DeviceSize offset = 0, vk::DeviceSize size = vk::WholeSize) {
    allocator().flushAllocation(allocation(), offset, size);
  }
  void invalidate(vk::DeviceSize offset = 0,
                  vk::DeviceSize size = vk::WholeSize) {
    allocator().invalidateAllocation(allocation(), offset, size);
  }

  void setName(const char *name) {
    allocator().setAllocationName(allocation(), name);
  }

private:
  vma::Allocator m_allocator;
  vma::Allocation m_allocation;
};
} // namespace detail

class Buffer : public detail::GpuAllocation {
public:
  Buffer(const Device &device,
         std::pair<vma::Allocation, vk::raii::Buffer> buffer,
         bool hasDeviceAddress = false, const char *name = nullptr);

  Buffer(const Device &device, const vk::BufferCreateInfo &bufferCreateInfo,
         const vma::AllocationCreateInfo &allocationCreateInfo,
         const char *name = nullptr);

  Buffer(const Device &device, vk::DeviceSize size,
         vk::BufferUsageFlags2KHR usage,
         const vma::AllocationCreateInfo &allocationCreateInfo =
             {{}, vma::MemoryUsage::eAuto},
         const char *name = nullptr, vk::BufferCreateFlags flags = {},
         vk::ArrayProxy<const uint32_t> queueFamilyIndices = {});

  vk::Buffer buffer() const { return *m_buffer; }
  operator vk::Buffer() const { return buffer(); }
  vk::Buffer operator*() const { return buffer(); }

  vk::DeviceAddress deviceAddress() const { return m_deviceAddress; }

  void setName(const Device &dev, const char *name) {
    dev.setDebugNameString(buffer(), name);
    detail::GpuAllocation::setName(name);
  }

  template <typename... Args>
  void setName(const Device &dev, std::format_string<Args...> fmt,
               Args &&...args) {
    if (dev.debugNamesAvaiable())
      setName(dev, std::format(fmt, std::forward<Args>(args)...).c_str());
  }

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

  Image(const Device &device, std::pair<vma::Allocation, vk::raii::Image> image,
        vk::ImageType imageType, vk::Format format, vk::Extent3D extent,
        uint32_t mipLevels, uint32_t arrayLayers,
        vk::SampleCountFlagBits samples, const char *name = nullptr);

  Image(const Device &device, const ImageCreateInfo &imageCreateInfo,
        const vma::AllocationCreateInfo &allocationCreateInfo,
        const char *name = nullptr);

  vk::Image image() const { return *m_image; }

  vk::ImageType imageType() const { return m_imageType; }
  vk::Format format() const { return m_format; }
  vk::Extent3D extent() const { return m_extent; }
  uint32_t mipLevels() const { return m_mipLevels; }
  uint32_t arrayLayers() const { return m_arrayLayers; }
  vk::SampleCountFlagBits samples() const { return m_samples; }

  void setName(const Device &dev, const char *name) {
    dev.setDebugNameString(image(), name);
    detail::GpuAllocation::setName(name);
  }

  template <typename... Args>
  void setName(const Device &dev, std::format_string<Args...> fmt,
               Args &&...args) {
    if (dev.debugNamesAvaiable())
      setName(dev, std::format(fmt, std::forward<Args>(args)...).c_str());
  }

private:
  vk::raii::Image m_image;

  vk::ImageType m_imageType;
  vk::Format m_format;
  vk::Extent3D m_extent;
  uint32_t m_mipLevels;
  uint32_t m_arrayLayers;
  vk::SampleCountFlagBits m_samples;
};
} // namespace v4dg