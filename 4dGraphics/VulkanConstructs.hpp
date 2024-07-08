#pragma once

#include "Device.hpp"

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cmath>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace v4dg {

class Buffer;
class Image;

namespace detail {

class GpuAllocation {
private:
  struct map_deleter_type {
    void operator()(void * /*unused*/) const {
      allocator.unmapMemory(allocation);
    }
    vma::Allocator allocator;
    vma::Allocation allocation;
  };

public:
  GpuAllocation(vma::Allocator allocator, vma::Allocation allocation)
      : m_allocator(allocator), m_allocation(allocation) {}

  // remove all movability as it is meant to represent immutable objects
  GpuAllocation() = delete;
  GpuAllocation(const GpuAllocation &) = delete;
  GpuAllocation &operator=(const GpuAllocation &) = delete;
  GpuAllocation(GpuAllocation &&) = delete;
  GpuAllocation &operator=(GpuAllocation &&) = delete;

  ~GpuAllocation() {
    if (m_allocation) {
      m_allocator.freeMemory(m_allocation);
    }
  }

  [[nodiscard]] vma::Allocator allocator() const noexcept {
    return m_allocator;
  }
  [[nodiscard]] vma::Allocation allocation() const noexcept {
    return m_allocation;
  }

  template <typename T> using map_type = std::unique_ptr<T[], map_deleter_type>;

  template <typename T> map_type<T> map() const {
    return map_type<T>(static_cast<T *>(allocator().mapMemory(allocation())),
                       {allocator(), allocation()});
  }

  void flush(vk::DeviceSize offset = 0,
             vk::DeviceSize size = vk::WholeSize) const {
    allocator().flushAllocation(allocation(), offset, size);
  }
  void invalidate(vk::DeviceSize offset = 0,
                  vk::DeviceSize size = vk::WholeSize) const {
    allocator().invalidateAllocation(allocation(), offset, size);
  }

  void setName(const char *name) const {
    allocator().setAllocationName(allocation(), name);
  }

private:
  vma::Allocator m_allocator;
  vma::Allocation m_allocation;
};

class BufferObject : public GpuAllocation {
private:
  struct internal_construct_t {};

public:
  BufferObject() = delete;
  BufferObject(const BufferObject &) = delete;
  BufferObject(BufferObject &&) = delete;
  BufferObject &operator=(const BufferObject &) = delete;
  BufferObject &operator=(BufferObject &&) = delete;

  [[nodiscard]] vk::Buffer buffer() const { return *m_buffer; }
  [[nodiscard]] vk::Buffer vk() const { return buffer(); }

  [[nodiscard]] vk::DeviceSize size() const { return m_size; }
  [[nodiscard]] vk::DeviceAddress deviceAddress() const {
    return m_deviceAddress;
  }

  template <typename... Args>
  void setName(const Device &dev, std::format_string<Args...> fmt,
               Args &&...args) const {
    if (dev.debugNamesAvaiable()) {
      std::string name = std::format(fmt, std::forward<Args>(args)...);
      dev.setDebugNameString(buffer(), name.c_str());
      detail::GpuAllocation::setName(name.c_str());
    }
  }

  BufferObject(internal_construct_t, const Device &device,
               const vk::BufferCreateInfo &bufferCreateInfo,
               const vma::AllocationCreateInfo &allocationCreateInfo);

  BufferObject(internal_construct_t, const Device &device, vk::DeviceSize size,
               vk::BufferUsageFlags2KHR usage,
               const vma::AllocationCreateInfo &allocationCreateInfo,
               vk::BufferCreateFlags flags,
               vk::ArrayProxy<const std::uint32_t> queueFamilyIndices);

private:
  BufferObject(internal_construct_t, const Device &device,
               std::pair<vma::Allocation, vk::raii::Buffer> buffer,
               vk::DeviceSize size, bool hasDeviceAddress);

  vk::raii::Buffer m_buffer;
  vk::DeviceSize m_size;
  vk::DeviceAddress m_deviceAddress;

  friend Buffer;
};

class ImageObject : public GpuAllocation {
private:
  struct internal_construct_t {};

public:
  struct ImageCreateInfo {
    vk::ImageCreateFlags flags = {};
    vk::ImageType imageType = vk::ImageType::e2D;
    vk::Format format;
    vk::Extent3D extent;
    std::uint32_t mipLevels = 1;
    std::uint32_t arrayLayers = 1;
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    vk::ImageUsageFlags usage;
    vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
    std::span<const std::uint32_t> queueFamilyIndices = {};
    vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined;

    // mutable formats
    std::optional<std::span<const vk::Format>> formats = {};

    std::optional<vk::ImageUsageFlags> stencilUsage = {};
  };

  ImageObject() = delete;
  ImageObject(const ImageObject &) = delete;
  ImageObject(ImageObject &&) = delete;
  ImageObject &operator=(const ImageObject &) = delete;
  ImageObject &operator=(ImageObject &&) = delete;

  [[nodiscard]] vk::Image image() const { return *m_image; }
  [[nodiscard]] vk::Image vk() const { return image(); }

  [[nodiscard]] vk::ImageType imageType() const { return m_imageType; }
  [[nodiscard]] vk::Format format() const { return m_format; }
  [[nodiscard]] vk::Extent3D extent() const { return m_extent; }
  [[nodiscard]] std::uint32_t mipLevels() const { return m_mipLevels; }
  [[nodiscard]] std::uint32_t arrayLayers() const { return m_arrayLayers; }
  [[nodiscard]] vk::SampleCountFlagBits samples() const { return m_samples; }

  template <typename... Args>
  void setName(const Device &dev, std::format_string<Args...> fmt,
               Args &&...args) const {
    if (dev.debugNamesAvaiable()) {
      std::string name = std::format(fmt, std::forward<Args>(args)...);
      dev.setDebugNameString(image(), name.c_str());
      detail::GpuAllocation::setName(name.c_str());
    }
  }

  ImageObject(internal_construct_t, const Device &device,
              std::pair<vma::Allocation, vk::raii::Image> image,
              vk::ImageType imageType, vk::Format format, vk::Extent3D extent,
              std::uint32_t mipLevels, std::uint32_t arrayLayers,
              vk::SampleCountFlagBits samples);

  ImageObject(internal_construct_t, const Device &device,
              const ImageCreateInfo &imageCreateInfo,
              const vma::AllocationCreateInfo &allocationCreateInfo);

private:
  vk::raii::Image m_image;

  vk::ImageType m_imageType;
  vk::Format m_format;
  vk::Extent3D m_extent;
  std::uint32_t m_mipLevels;
  std::uint32_t m_arrayLayers;
  vk::SampleCountFlagBits m_samples;

  friend Image;
};
} // namespace detail

class Buffer : public std::shared_ptr<const detail::BufferObject> {
public:
  Buffer() = delete;

  Buffer(const Device &device, const vk::BufferCreateInfo &bufferCreateInfo,
         const vma::AllocationCreateInfo &allocationCreateInfo);

  Buffer(const Device &device, vk::DeviceSize size,
         vk::BufferUsageFlags2KHR usage,
         const vma::AllocationCreateInfo &allocationCreateInfo =
             {{}, vma::MemoryUsage::eAuto},
         vk::BufferCreateFlags flags = {},
         vk::ArrayProxy<const std::uint32_t> queueFamilyIndices = {});
};

class Image : public std::shared_ptr<const detail::ImageObject> {
public:
  using ImageCreateInfo = detail::ImageObject::ImageCreateInfo;

  Image() = delete;

  Image(const Device &device, const ImageCreateInfo &imageCreateInfo,
        const vma::AllocationCreateInfo &allocationCreateInfo);
};

} // namespace v4dg