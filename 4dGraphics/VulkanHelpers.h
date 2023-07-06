#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#define VMA_STATS_STRING_ENABLED 0
#include <volk.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <queue>
#include <functional>
#include <vk_mem_alloc.h>
#include <shared_mutex>
#include <Debug.h>
#include <stdint.h>
#include "cppHelpers.hpp"
#include "descriptor_allocator.h"
#include <taskflow/taskflow.hpp>

namespace vulkan_helpers {
	template <typename T, typename F, typename... Ts> auto get_vector( std::vector<T> &out, F &&f, Ts&&... ts ) -> VkResult
	{
		uint32_t count = 0;
		VkResult err;
		do {
			err = f( ts..., &count, nullptr );
			if( err != VK_SUCCESS ) return err;
			out.resize( count );
			err = f( ts..., &count, out.data() );
			out.resize( count );
		} while( err == VK_INCOMPLETE );
		return err;
	}

	template <typename T, typename F, typename... Ts> auto get_vector_noerror( F &&f, Ts&&... ts ) -> std::vector<T>
	{
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
	const VkBaseOutStructure *get_vk_structure( const void *pNextChain, VkStructureType sType );
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

VkResult FindSuitablePhysicalDevice( VkInstance instance,
	std::function<bool( VkPhysicalDevice )> selector, VkPhysicalDevice *physicalDevice
);
uint32_t FindQueueFamilies( const std::vector<VkQueueFamilyProperties> &families, 
	VkQueueFlags neededFlags, 
	VkQueueFlags wantedFlags = 0, 
	VkQueueFlags unwantedFlags = 0,
	VkQueueFlags forbiddenFlags = 0
);

// swapchain creation functions
VkSurfaceFormatKHR ChooseSwapchainFormat( VkPhysicalDevice pd, VkSurfaceKHR surf, VkFormatFeatureFlags features );
VkPresentModeKHR ChooseSwapPresentMode( VkPhysicalDevice pd, VkSurfaceKHR surf, bool VSync, bool limitFramerate );
uint32_t ChooseSwapImageCount( uint32_t wantedImageCount, const VkSurfaceCapabilitiesKHR &capabilities );
VkExtent2D ChooseSwapchainExtent( VkExtent2D defaultSize, const VkSurfaceCapabilitiesKHR &capabilities );

VkResult CreateSwapchain(
	VkDevice device, VkSurfaceKHR surface, 
	VkExtent2D swapchainSize, uint32_t swapchainLayers, uint32_t minImageCount,
	VkSurfaceFormatKHR surfaceFormat, VkImageUsageFlags swapchainUsage, 
	VkSurfaceTransformFlagBitsKHR preTransform, VkCompositeAlphaFlagBitsKHR compositeAlpha,
	VkPresentModeKHR presentMode, VkBool32 clipped,
	VkSwapchainKHR oldSwapchain, VkSwapchainKHR *swapchain
);

VkResult CreateSwapchainImages( VkDevice device, VkSwapchainKHR swapchain,
	uint32_t *swapchainImagesCount, VkFormat imageFormat,
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