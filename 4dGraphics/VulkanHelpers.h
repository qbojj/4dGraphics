#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <volk.h>
#include <vector>
#include <functional>
#include <vk_mem_alloc.h>

namespace vulkan_helpers {
	template <typename T, typename F, typename... Ts> auto get_vector( std::vector<T> &out, F &&f, Ts&&... ts ) -> VkResult
	{
		uint32_t count = 0;
		VkResult err;
		do {
			err = f( ts..., &count, nullptr );
			if( err != VK_SUCCESS ) {
				return err;
			};
			out.resize( count );
			err = f( ts..., &count, out.data() );
			out.resize( count );
		} while( err == VK_INCOMPLETE );
		return err;
	}

	template <typename T, typename F, typename... Ts> auto get_vector_noerror( F &&f, Ts&&... ts ) -> std::vector<T> {
		uint32_t count = 0;
		std::vector<T> results;
		f( ts..., &count, nullptr );
		results.resize( count );
		f( ts..., &count, results.data() );
		results.resize( count );
		return results;
	}

	VkResult enumerate_instance_extensions( std::vector<VkExtensionProperties> &exts, const std::vector<const char *> &layers );
	VkResult enumerate_device_extensions( std::vector<VkExtensionProperties> &exts, VkPhysicalDevice phDev, const std::vector<const char *> &layers );
	uint32_t is_extension_present( const std::vector<VkExtensionProperties> &exts, const char *pName ); // returns found spec version or 0 if not present
	bool is_extension_present( const std::vector<const char *> &exts, const char *pName );
}

// high level interfaces
struct VulkanInstance {
	VkInstance instance;
	uint32_t apiVersion;
	VkSurfaceKHR surface;
	std::vector<const char *> enabledLayers;
	std::vector<const char *> enabledExts;
	VkDebugUtilsMessengerEXT messenger;
};

struct VulkanQueue {
	VkQueue queue;
	uint32_t family;
};

struct VulkanDevice {
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VmaAllocator allocator;
	std::vector<const char *> enabledExts;
	VkPipelineCache pipelineCache;
};

struct VulkanRenderDevice {
	VulkanDevice device; // non owning
	VulkanQueue graphicsQueue;
	VkSemaphore semophore;
	VkSemaphore renderSemaphore;
	VkFence fence;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
};

struct VulkanTexture {
	VkImage image;
	VmaAllocation imageMemory;
	VkImageView imageView;
};

struct VulkanBuffer {
	VkBuffer buffer;
	VmaAllocation bufferAllocation;
};

struct VulkanBufferSuballocation {
	VkDeviceSize offset;
	VkDeviceSize size;
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

// for caches and such
VK_DEFINE_NON_DISPATCHABLE_HANDLE( VulkanBudgetAllocator )
VkResult CreateVulkanBudgetAllocator( VkDeviceSize size, VulkanBudgetAllocator *pBudgetAllocator );
void DestroyVulkanBudgetAllocator( VulkanBudgetAllocator budgetAllocator );
void GetVulkanBudgetCallbacks( VulkanBudgetAllocator budgetAllocator, VkAllocationCallbacks *pCallbacks );

VK_DEFINE_HANDLE( VulkanPNextChainManager )
VkResult CreateVulkanPNextChainManager( VulkanPNextChainManager *pManager );
void DestroyVulkanPNextChainManager( VulkanPNextChainManager manager );
VkResult AddVulkanPNextStructure( VulkanPNextChainManager manager, VkStructureType type, VkDeviceSize size );
VkResult AllocateVulkanPNextChain( VulkanPNextChainManager manager, VkBaseOutStructure **ppNextChain );
void FreeVulkanPNextChain( VulkanPNextChainManager manager, VkBaseOutStructure *pNextChain );
void GetVulkanPNextChainStructureCount( VulkanPNextChainManager manager, VkStructureType type, uint32_t *pCount );

VkResult InitVulkanDevice(
	const VulkanInstance &vk,
	VkPhysicalDevice device,
	const std::vector<VkDeviceQueueCreateInfo> &families,
	const std::vector<const char *> &extensions,
	const VkPhysicalDeviceFeatures2 *deviceFeatures, 
	VulkanDevice &vkDev
);

VkResult InitVulkanRenderDevice( 
	const VulkanInstance &vk, VulkanDevice &vkDev,
	VulkanQueue graphicsQueue,
	uint32_t width, uint32_t height,
	VulkanRenderDevice &vkRDev
);

VkResult CreateTextureImage( VulkanRenderDevice &vkDev,
	const char *filename, VulkanTexture *texture 
);

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

void DestroyVulkanState( VulkanDevice &vkDev, VulkanState &vkState );
void DestroyVulkanRendererDevice( VulkanRenderDevice &vkDev );
void DestroyVulkanDevice( VulkanDevice &vkDev );
void DestroyVulkanInstance( VulkanInstance &vk );

void DestroyVulkanTexture( const VulkanDevice &device, VulkanTexture &texture );

VkResult BeginSingleTimeCommands( VkDevice device, VkCommandPool commandPool, VkCommandBuffer *commandBuffer );
VkResult EndSingleTimeCommands( VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer );

// instance and device creation functions

VkResult CreateInstance(
	const std::vector<const char *> &layers,
	const std::vector<const char *> &extensions, 
	const VkApplicationInfo *pAppInfo, // may be NULL
	bool enumeratePortability,
	const void *pCreateInstanceNext,
	VkInstance *pInstance 
);
VkResult CreateDevice( VkPhysicalDevice physicalDevice,
	const std::vector<VkDeviceQueueCreateInfo> &families,
	const std::vector<const char *> &extensions,
	const VkPhysicalDeviceFeatures2 *deviceFeatures, VkDevice *pDevice
);

VkResult FindSuitablePhysicalDevice( VkInstance instance,
	std::function<bool( VkPhysicalDevice )> selector, VkPhysicalDevice *physicalDevice
);
uint32_t FindQueueFamilies( VkPhysicalDevice device, VkQueueFlags desiredFlags );

// debug functions
VkResult SetVkObjectName( VkDevice vkDev, uint64_t object, VkObjectType objType, const char *name );

// swapchain creation functions

struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities = {};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

VkResult QuerySwapchainSupport( VkPhysicalDevice device, VkSurfaceKHR surface, SwapchainSupportDetails *details );
VkSurfaceFormatKHR ChooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR> &avaiableFormats );
VkPresentModeKHR ChooseSwapPresentMode( const std::vector<VkPresentModeKHR> &avaiablePresentModes );
uint32_t ChooseSwapImageCount( const VkSurfaceCapabilitiesKHR &capabilities );

VkResult CreateSwapchain(
	VkDevice device, VkPhysicalDevice physicalDevice,
	VkSurfaceKHR surface, uint32_t width, uint32_t height,
	VkSwapchainKHR *swapchain
);

VkResult CreateSwapchainImages( VkDevice device, VkSwapchainKHR swapchain,
	uint32_t *swapchainImagesCount,
	std::vector<VkImage> &swapchainImages, std::vector<VkImageView> &swapchainImageViews
);

// memory/buffer manipulation functions

void GetSuballocatedBufferSize( 
	const std::vector<VkDeviceSize> &sizes,
	VkDeviceSize alignment, 
	VkDeviceSize *bufferSize,
	std::vector<VulkanBufferSuballocation> &suballocations
);

VkResult CreateBuffer(
	VmaAllocator allocator,
	VkDeviceSize size, VkBufferCreateFlags flags, VkBufferUsageFlags usage,
	VmaAllocationCreateFlags allocationFlags, VmaMemoryUsage vmaUsage,
	VkBuffer *buffer, VmaAllocation *bufferMemory, VmaAllocationInfo *allocationInfo
);

// image manipulation functions

VkResult CreateImageResource(
	VulkanDevice &vkDev,
	VkFormat format, VkImageType imageType,
	VkExtent3D size, uint32_t mipLevels, uint32_t arrayLayers,
	VkSampleCountFlagBits samples, VkImageTiling tiling,
	VkImageCreateFlags flags, VkImageUsageFlags usage,
	VmaAllocationCreateFlags allocationFlags, VmaMemoryUsage vmaUsage,
	VulkanTexture *imageResource
);

VkResult CreateImage(
	VmaAllocator allocator,
	VkFormat format, VkImageType imageType,
	VkExtent3D size, uint32_t mipLevels, uint32_t arrayLayers,
	VkSampleCountFlagBits samples, VkImageTiling tiling,
	VkImageCreateFlags flags, VkImageUsageFlags usage,
	VmaAllocationCreateFlags allocationFlags, VmaMemoryUsage vmaUsage,
	VkImage *image, VmaAllocation *imageMemory, VmaAllocationInfo *allocationInfo
);
VkResult CreateImageView( 
	VkDevice device, VkImage image, 
	VkFormat format, VkImageAspectFlags aspectFlags, VkImageView *imageView
);
VkResult CreateTextureSampler( VkDevice device,
	VkFilter filter, VkSamplerMipmapMode mipMode,
	VkSamplerAddressMode addressU, VkSamplerAddressMode addressV, VkSamplerAddressMode addressW,
	float anisotropy,
	VkSampler *sampler
);
void CopyBufferToImageCmd(
	VkCommandBuffer cmdBuffer,
	VkBuffer srcBuffer, VkDeviceSize srcOffset,
	VkImage dstImage, VkExtent3D size, VkOffset3D offset
);
void TransitionImageLayoutCmd(
	VkCommandBuffer cmdBuffer,
	VkImage image, VkImageAspectFlags aspects,
	VkImageLayout oldLayout, VkImageLayout newLayout,
	VkPipelineStageFlags srcStageMask, VkAccessFlags srcAccessMask,
	VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask
);

// returns VK_FORMAT_UNDEFINED if cannot find
VkFormat FindSupportedFormat(
	VkPhysicalDevice device,
	const std::vector<VkFormat> &candidates,
	VkImageTiling tiling, VkFormatFeatureFlags features
);
VkFormat FindDepthFormat( VkPhysicalDevice device );
VkImageAspectFlags FormatGetAspects( VkFormat fmt );
// synchronization functions

VkResult CreateSemophore( VkDevice device, VkSemaphore *semaphore );
VkResult CreateTimelineSemaphore( VkDevice device, uint64_t initialValue, VkSemaphore *semaphore );

// descriptor set functions
VkResult CreateDescriptorPool(
	VkDevice device,
	void *pNext,
	VkDescriptorPoolCreateFlags flags,
	uint32_t maxSets,
	uint32_t poolSizeCount,
	const VkDescriptorPoolSize *pPoolSizes,
	VkDescriptorPool *descPool
);
VkResult CreateDescriptorSetHelper(
	VkDevice device,
	VkDescriptorPoolCreateFlags flags,
	uint32_t maxSets,
	uint32_t uniformBuffersPerSet,
	uint32_t storageBuffersPerSet,
	uint32_t imageSamplersPerSet,
	VkDescriptorPool *descPool
);

// shader creation functions
VkResult CreateShaderModule( 
	VkDevice device, 
	const char *filename, 
	VkShaderModuleCreateFlags flags,
	const void *pNext,
	VkShaderModule *shaderModule 
);

VkResult CreatePipelineLayout(
	VkDevice device,
	uint32_t setLayoutCount,
	const VkDescriptorSetLayout *pSetLayouts,
	uint32_t pushConstantRangeCount,
	const VkPushConstantRange *pPushConstantRanges,
	VkPipelineLayout *layout
);

void FillGraphicsPipelineDefaults( VkGraphicsPipelineCreateInfo *gpci );
VkPipelineShaderStageCreateInfo FillShaderStage( 
	VkShaderStageFlagBits stage, 
	VkShaderModule module, const char *name 
);