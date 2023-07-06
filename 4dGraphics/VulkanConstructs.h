#pragma once

#include <functional>
#include <memory>
#include <span>
#include "VulkanHelpers.h"
#include "VulkanCaches.hpp"
#include "vulkanTypeTraits.hpp"
#include "descriptor_allocator.h"

typedef std::function<void()> DestructionQueueItem;

class VulkanPDInfo {
public:
	VulkanPDInfo(); // fill only sType and pNext
	VulkanPDInfo( VkPhysicalDevice );
	VulkanPDInfo( const VulkanPDInfo & );
	VulkanPDInfo&operator=(VulkanPDInfo);
	
	// properties
	VkPhysicalDeviceProperties2 props;
	VkPhysicalDeviceVulkan11Properties props11;
	VkPhysicalDeviceVulkan12Properties props12;
	VkPhysicalDeviceVulkan13Properties props13;
	
	// features
	VkPhysicalDeviceFeatures2 feats;
	VkPhysicalDeviceVulkan11Features feats11;
	VkPhysicalDeviceVulkan12Features feats12;
	VkPhysicalDeviceVulkan13Features feats13;

	VkPhysicalDeviceMemoryProperties memory;

	void clear_core_features();

private:
	void setup();
};

// high level interfaces
class VulkanInstance {
public:
    VulkanInstance( 
        std::vector<const char *> layers,
	    std::vector<const char *> extensions,
	    const VkApplicationInfo *pAppInfo,
	    bool enumeratePortability = false,
	    const void *pCreateInstanceNext = nullptr );

    ~VulkanInstance();

	VkInstance instance;
	uint32_t apiVersion;
	std::vector<const char *> enabledLayers;
	std::vector<const char *> enabledExts;
	VkDebugUtilsMessengerEXT messenger;

private:
    void clear();
};

// avoid false sharing using alignment
struct alignas(64) VulkanQueue {
	mutable std::mutex mut;
    VkQueue queue;
	uint32_t family;
};

class VulkanDevice {
public:
    VulkanDevice(
        const VulkanInstance &vk,
        VkPhysicalDevice physicalDevice,
        std::span<const VkDeviceQueueCreateInfo> families,
        std::vector<const char *> extensions,
        const VkPhysicalDeviceFeatures2 *deviceFeatures,
        std::string name );

    ~VulkanDevice();

    std::string name;
	
    VkPhysicalDevice physicalDevice;
	VkDevice device;
	VmaAllocator allocator;

	std::vector<const char *> enabledExts;
	std::vector<VulkanQueue> queues;

	VkPipelineCache pipelineCache;

	VulkanPDInfo info;

private:
    static size_t get_queue_count(std::span<const VkDeviceQueueCreateInfo>);
    void clear();
    void serialize();
};

class VulkanCommandBufferManager : private cpph::move_only {
public:
	VulkanCommandBufferManager( const VulkanDevice &dev, uint32_t family, const std::string &name = "",
        VkCommandPoolCreateFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
    VulkanCommandBufferManager(VulkanCommandBufferManager&&) = default;
	~VulkanCommandBufferManager();

    VulkanCommandBufferManager&operator=(VulkanCommandBufferManager&&) = default;

	VkResult reset( VkCommandPoolResetFlags flags = 0 );
	VkResult get( VkCommandBufferLevel level, VkCommandBuffer *cmdBuff );

	VkCommandPool cmdPool = VK_NULL_HANDLE;

private:
	VkDevice dev;
	std::vector<VkCommandBuffer> prim, sec;
	size_t usedPrim = 0, usedSec = 0;

    void clear();
};

class VulkanContext;

class VulkanPerThread {
public:
    VulkanPerThread( const VulkanDevice &, std::span<const uint32_t> families, const std::string &name );
	~VulkanPerThread();

	std::vector<VulkanCommandBufferManager> cmdBuffManagers; // one for every queue family
};

class VulkanPerFrame {
public:
	void add_destr_fn(std::function<void()>);

	std::vector<uint64_t> readyResources;
    VkSemaphore imageReady, readyForPresent; // binary

	tf::CriticalSection threadCS;
	std::vector<VulkanPerThread> threadData;

private:
	VulkanPerFrame( const VulkanDevice &dev, uint32_t queueCount );
    VulkanPerFrame( const VulkanDevice &dev, uint32_t queueCount, uint32_t threadCount, 
        std::span<const uint32_t> families, const std::string &name );
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
		VkColorSpaceKHR colorSpace; // SRGB for SRGB format and PASS_THROUGH otherwise
		VkImageUsageFlags usages;
	};

	// takes current VulkanWSI, device, surface, and old swapchain if it exists
	typedef std::function<WSIData(const VulkanWSI&, const VulkanDevice &, VkSurfaceKHR, VkSwapchainKHR)> fn_recreateSwp;

	// create WSI backed
    VulkanWSI(VulkanContext &, const std::string &name, fn_recreateSwp recreate, VkSurfaceKHR surface );

    // create backbuffer backend
    VulkanWSI(VulkanContext &, const std::string &name, const BackbufferData &data );
	~VulkanWSI();

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
	VulkanContext &ctx;
};
*/

class VulkanContext {
public:
	VulkanContext( const VulkanInstance &vk, const VulkanDevice &vkDev, uint32_t framesInFlight, uint32_t threadCount, std::string name );
	~VulkanContext();

	VulkanInstance &vk;
	VulkanDevice &vkDev;

private:
	std::string name;
	std::vector<VkDescriptorPoolSize> poolSizes;
	VkDescriptorPoolCreateInfo dpci;

public:
	//DSAlloc::VulkanDSAllocatorPool descPool;

	uint64_t currentFrameId;
	uint32_t frameIdx;
	std::vector<VulkanPerFrame> perFrame;

	HDesc::SamplerCache sampler_cache;
	HDesc::DescriptorSetLayoutCache dsl_cache;
	HDesc::PipelineLayoutCache layout_cache;

private:
	std::vector<VulkanPerFrame> create_per_frame(uint32_t,uint32_t) const;
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

VkResult InitVulkanContext( 
	VulkanInstance &vk, VulkanDevice &vkDev,
	uint32_t WantedFramesInFlight, VkExtent2D wantedExtent, 
	VkImageUsageFlags usage, VkFormatFeatureFlags features,
	uint32_t threadCount, VulkanContext &vkCtx
);

VkResult CreateTextureImage( VulkanThreadCtx &vkCtx,
	const char *filename, VulkanTexture *texture 
);

/*
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
*/
void DestroyVulkanState( VulkanDevice &vkDev, VulkanState &vkState );
void DestroyVulkanContext( VulkanContext &vkDev );

//void DestroyVulkanSwapchain( VkDevice device, VulkanSwapchain &swapchain );

VkResult CreateImageResource(
	VulkanDevice &vkDev,
	VkFormat format, VkImageType imageType,
	VkExtent3D size, uint32_t mipLevels, uint32_t arrayLayers,
	VkSampleCountFlagBits samples, VkImageTiling tiling,
	VkImageCreateFlags flags, VkImageUsageFlags usage,
	VmaAllocationCreateFlags allocationFlags, VmaMemoryUsage vmaUsage,
	VulkanTexture *imageResource
);
void DestroyVulkanTexture( const VulkanDevice &device, VulkanTexture &texture );

template<typename T>
inline void set_vk_name(const VulkanDevice &dev, T h, const std::string &name )
{
    if constexpr ( !IS_DEBUG ) return;
    
    const VkDebugUtilsObjectNameInfoEXT nameInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = vulkan_traits::traits<T>::objtype,
		.objectHandle = (uint64_t)h,
		.pObjectName = name.c_str()
	};

    CHECK_THROW( vkSetDebugUtilsObjectNameEXT( dev.device, &nameInfo ), "could not set name for " + name );
}
