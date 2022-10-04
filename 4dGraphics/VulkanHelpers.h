#pragma once

#include <volk.h>
#include <vector>
#include <functional>
#include <vk_mem_alloc.h>

// high level interfaces
struct VulkanInstance {
	VkInstance instance;
	VkSurfaceKHR surface;
	VkDebugUtilsMessengerEXT messenger;
	VkDebugReportCallbackEXT reportCallback;
};

struct VulkanQueue {
	VkQueue queue;
	uint32_t family;
};

struct VulkanDevice {
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VmaAllocator allocator;
};

struct VulkanRenderDevice {
	VulkanDevice *device; // non owning
	VulkanQueue graphicsQueue;
	VkSemaphore semophore;
	VkSemaphore renderSemaphore;
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
	std::vector<VkDescriptorSet> descriptorSets;

	VulkanBuffer uniformBuffer;
	std::vector<VulkanBufferSuballocation> uniformBuffers;

	VulkanBuffer modelBuffer;
	VulkanBufferSuballocation vertexBuffer, indexBuffer;
	VkSampler textureSampler;
	VulkanTexture texture;
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
	VulkanInstance &vk,
	std::function<bool( VkPhysicalDevice )> selector,
	const VkPhysicalDeviceFeatures2 *deviceFeatures, 
	VulkanDevice &vkDev
);
VkResult InitVulkanRenderDevice( 
	const VulkanInstance &vk, VulkanDevice &vkDev,
	uint32_t width, uint32_t height,
	VulkanRenderDevice &vkRDev
);

VkResult CreateDepthResource( VulkanRenderDevice &vkDev,
	uint32_t width, uint32_t height,
	VulkanTexture *depth 
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

void DestroyVulkanRendererDevice( VulkanRenderDevice &vkDev );
void DestroyVulkanDevice( VulkanDevice &vkDev );
void DestroyVulkanInstance( VulkanInstance &vk );

void DestroyVulkanTexture( const VulkanDevice &device, VulkanTexture &texture );

VkResult BeginSingleTimeCommands( VkDevice device, VkCommandPool commandPool, VkCommandBuffer *commandBuffer );
VkResult EndSingleTimeCommands( VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer );

// instance and device creation functions

VkResult RemoveUnsupportedLayers( std::vector<const char *> &layers );
VkResult RemoveUnsupportedExtensions( const std::vector<const char *> &layers, std::vector<const char *> &exts );

// pInstanceCreateInfo may be NULL
VkResult CreateInstance( const VkInstanceCreateInfo *pInstanceCreateInfo, VkInstance *pInstance );
VkResult CreateDevice( VkPhysicalDevice physicalDevice, 
	const VkPhysicalDeviceFeatures2 *deviceFeatures, uint32_t graphicsFamily, VkDevice *pDevice
);

VkResult FindSuitablePhysicalDevice( VkInstance instance,
	std::function<bool( VkPhysicalDevice )> selector, VkPhysicalDevice *physicalDevice
);
uint32_t FindQueueFamilies( VkPhysicalDevice device, VkQueueFlags desiredFlags );

// debug functions
VkResult SetupDebugCallbacks( VkInstance instance,
	VkDebugUtilsMessengerEXT *messenger, PFN_vkDebugUtilsMessengerCallbackEXT messengerCallback, void *messengerUserData,
	VkDebugReportCallbackEXT *reportCallback, PFN_vkDebugReportCallbackEXT reportMessageCallback, void *reportUserData
);
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
	VkDevice device, VkPhysicalDevice physicalDevice, uint32_t graphicsFamily,
	VkSurfaceKHR surface, uint32_t width, uint32_t height,
	VkSwapchainKHR *swapchain
);

VkResult CreateSwapchainImages( VkDevice device, VkSwapchainKHR swapchain,
	uint32_t *swapchainImagesCount,
	std::vector<VkImage> &swapchainImages, std::vector<VkImageView> &swapchainImageViews
);

// memory/buffer manipulation functions

VkResult CreateBuffer(
	VmaAllocator allocator,
	VkDeviceSize size, VkBufferCreateFlags flags, VkBufferUsageFlags usage,
	VmaAllocationCreateFlags allocationFlags, VmaMemoryUsage vmaUsage,
	VkBuffer *buffer, VmaAllocation *bufferMemory, VmaAllocationInfo *allocationInfo
);

// image manipulation functions

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
	VkImage image, VkFormat format,
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
bool FormatHasDepthComponent( VkFormat fmt );
bool FormatHasStencilComponent( VkFormat fmt );
// synchronization functions

VkResult CreateSemophore( VkDevice device, VkSemaphore *semaphore );

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