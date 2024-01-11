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
/*
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
  BufferCreateInfo m_bufferCreateInfo;

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
*/
/*
// can be backed by real WSI or user image (e.g for recording)
class VulkanWSI : private cpph::move_only {
public:
    struct WSIData {
        VkSwapchainKHR swapchain;

        VkExtent2D extent;
        uint32_t imageLayerCnt;
        VkSurfaceTransformFlagBitsKHR preTransform;
        VkCompositeAlphaFlagBitsKHR compositeAlpha;

        VkFormat format;
        VkColorSpaceKHR colorSpace;
        VkImageUsageFlags usages;
    };

    // creates VkBuffer of texture to read from before
    struct BackbufferData {
        uint32_t imageCount;
        VkExtent2D extent;
        uint32_t imageLayerCnt;

        VkFormat format;
        VkColorSpaceKHR colorSpace;
        VkImageUsageFlags usages;
    };

    // takes current VulkanWSI, device, surface, and old swapchain if it
    exists typedef std::function<WSIData(const VulkanWSI&, const VulkanDevice&,
        VkSurfaceKHR, VkSwapchainKHR)>
        fn_recreateSwp;

    // create WSI backed
    VulkanWSI(VulkanContext&, const std::string& name, fn_recreateSwp recreate,
        VkSurfaceKHR surface);

    // create backbuffer backend
    VulkanWSI(VulkanContext&, const std::string& name, const BackbufferData&
data); ~VulkanWSI();

    void advance_frame(VkSemaphore imageReady);
    void finish_frame(VkSemaphore readyForPresent);

    uint32_t currentImageIdx;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;

    VkExtent2D extent;
    uint32_t imageLayers;

    VkSurfaceTransformFlagBitsKHR preTransform;
    VkCompositeAlphaFlagBitsKHR compositeAlpha;

    VkFormat format;
    VkColorSpaceKHR colorSpace;
    VkImageUsageFlags usages;

    const std::string name;

private:
    struct WSISwapchain {
        VkSurfaceKHR surface;
        VkSwapchainKHR swapchain;

        fn_recreateSwp recreate;
    };

    struct BackbufferSwapchain {
        VulkanBuffer buffer;
        std::vector<VulkanBufferSuballocation> frames;
        std::vector<uint64_t> frameTime;
    };

    std::variant<WSISwapchain, BackbufferSwapchain> swapchainData;
    VulkanContext& ctx;
};

class VulkanPerThread {
public:
  VulkanPerThread(const VulkanDevice &, std::span<const uint32_t> families,
                  const std::string &name);
  ~VulkanPerThread();

  std::vector<command_buffer_manager>
      cmdBuffManagers; // one for every queue family
};

class VulkanPerFrame {
public:
  void add_destr_fn(std::function<void()>);

  std::vector<uint64_t> readyResources;
  vk::Semaphore imageReady, readyForPresent; // binary

  tf::CriticalSection threadCS;
  std::vector<VulkanPerThread> threadData;

private:
  VulkanPerFrame(const VulkanDevice &dev, uint32_t queueCount);
  VulkanPerFrame(const VulkanDevice &dev, uint32_t queueCount,
                 uint32_t threadCount, std::span<const uint32_t> families,
                 const std::string &name);
  ~VulkanPerFrame();

  void advance_frame();
  uint32_t get_thread_idx();
  void release_thread_idx(uint32_t);

  std::atomic<uint32_t> lastTID;
  std::mutex mut_;
  std::queue<DestructionQueueItem> destructionQueue;

  void eval_queue();
  void clear();
  VkDevice dev;

  friend VulkanContext;
};

class VulkanContext {
public:
  VulkanContext(const VulkanInstance &vk, const VulkanDevice &vkDev,
                uint32_t framesInFlight, uint32_t threadCount,
                std::string name);
  ~VulkanContext();

  VulkanInstance &vk;
  VulkanDevice &vkDev;

private:
  std::string name;
  std::vector<VkDescriptorPoolSize> poolSizes;
  VkDescriptorPoolCreateInfo dpci;

public:
  // DSAlloc::VulkanDSAllocatorPool descPool;

  uint64_t currentFrameId;
  uint32_t frameIdx;
  std::vector<VulkanPerFrame> perFrame;

  HDesc::SamplerCache sampler_cache;
  HDesc::DescriptorSetLayoutCache dsl_cache;
  HDesc::PipelineLayoutCache layout_cache;

private:
  std::vector<VulkanPerFrame> create_per_frame(uint32_t, uint32_t) const;
};

class VulkanThreadCtx {
public:
  VulkanThreadCtx(VulkanContext &);
  ~VulkanThreadCtx();

  VulkanContext &ctx;
  VulkanPerFrame &fCtx;

  uint32_t threadId;
  VulkanPerThread &tCtx;
};

struct VulkanState {
  VkDescriptorPool descriptorPool;
  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorSet descriptorSet;

  VulkanBuffer uniformBufferMemory;
  std::vector<VulkanBufferSuballocation> uniformBuffers;

  VulkanBuffer modelBuffer;
  VulkanBufferSuballocation vertexBuffer, indexBuffer;
  VkSampler textureSampler;
  VulkanTexture texture;

  VkPipelineLayout layout;
  VkPipeline graphicsPipeline;

  VulkanTexture depthResource;
};

VkResult InitVulkanContext(VulkanInstance &vk, VulkanDevice &vkDev,
                           uint32_t WantedFramesInFlight,
                           VkExtent2D wantedExtent, VkImageUsageFlags usage,
                           VkFormatFeatureFlags features, uint32_t threadCount,
                           VulkanContext &vkCtx);

VkResult CreateTextureImage(VulkanThreadCtx &vkCtx, const char *filename,
                            VulkanTexture *texture);


VkResult CreateSSBOVertexBuffer( VulkanRenderDevice &vkDev,
        const char *filename,
        VkBuffer *storageBuffer, VmaAllocation *storageBufferMemory,
        VulkanBufferSuballocation *vertexBuffer,
        VulkanBufferSuballocation *indexBuffer
);

VkResult CreateEngineDescriptorSetLayout(
        VkDevice device,
        VkDescriptorSetLayout *layout
);

VkResult CreateEngineDescriptorSets(
        VulkanRenderDevice &vkDev,
        VulkanState &vkState
);

void DestroyVulkanState(VulkanDevice &vkDev, VulkanState &vkState);
void DestroyVulkanContext(VulkanContext &vkDev);
*/
} // namespace v4dg