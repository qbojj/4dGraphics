#include "VulkanHelpers.h"
#include "Debug.h"
#include "Shader.h"

#include <vector>
#include <algorithm>
#include <functional>

#include <CommonUtility.h>
#include <assimp/scene.h>
#include <assimp/cimport.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb_image.h>

using namespace std;
using namespace vulkan_helpers;

VkResult InitVulkanDevice(
	VulkanInstance &vk,
	VkPhysicalDevice device,
	const std::vector<VkDeviceQueueCreateInfo> &families,
	const std::vector<const char *> &extensions,
	const VkPhysicalDeviceFeatures2 *deviceFeatures, 
	VulkanDevice &vkDev
)
{
	VmaAllocatorCreateInfo aci{};
	VmaVulkanFunctions functions{};
	
	VK_CHECK_GOTO_INIT;
	vkDev = {};

	vkDev.physicalDevice = device;
	
	VK_CHECK_GOTO( CreateDevice( vkDev.physicalDevice, families, extensions, deviceFeatures, &vkDev.device ) );

	aci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	aci.instance = vk.instance;
	aci.physicalDevice = vkDev.physicalDevice;
	aci.device = vkDev.device;
	aci.vulkanApiVersion = vk.apiVersion;
	functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

	aci.pVulkanFunctions = &functions;
	
	VK_CHECK_GOTO( vmaCreateAllocator( &aci, &vkDev.allocator ) );
	return VK_SUCCESS;

	VK_CHECK_GOTO_HANDLE( ret );
	DestroyVulkanDevice( vkDev );
	return ret;
}

VkResult InitVulkanRenderDevice( 
	const VulkanInstance &vk, VulkanDevice &vkDev,
	VulkanQueue graphicsQueue,
	uint32_t width, uint32_t height,
	VulkanRenderDevice &vkRDev )
{
	uint32_t imageCount;
	VkCommandPoolCreateInfo cpCI;
	VkCommandBufferAllocateInfo cbAI;
	
	VK_CHECK_GOTO_INIT;

	vkRDev = {};
	vkRDev.device = vkDev;
	vkRDev.graphicsQueue = graphicsQueue;

	VK_CHECK_GOTO( CreateSwapchain(
		vkDev.device, vkDev.physicalDevice, vkRDev.graphicsQueue.family, vk.surface,
		width, height, &vkRDev.swapchain ) );


	VK_CHECK_GOTO( CreateSwapchainImages(
		vkDev.device, vkRDev.swapchain, &imageCount,
		vkRDev.swapchainImages, vkRDev.swapchainImageViews ) );

	vkRDev.commandBuffers.resize( imageCount );


	VK_CHECK_GOTO( CreateSemophore( vkDev.device, &vkRDev.semophore ) );
	VK_CHECK_GOTO( CreateSemophore( vkDev.device, &vkRDev.renderSemaphore ) );

	cpCI = VkCommandPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.queueFamilyIndex = vkRDev.graphicsQueue.family
	};

	VK_CHECK_GOTO( vkCreateCommandPool( vkDev.device, &cpCI, nullptr, &vkRDev.commandPool ) );

	cbAI = VkCommandBufferAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = vkRDev.commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = (uint32_t)vkRDev.commandBuffers.size()
	};

	VK_CHECK_GOTO( vkAllocateCommandBuffers( vkDev.device, &cbAI, vkRDev.commandBuffers.data() ) );

	return VK_SUCCESS;

	VK_CHECK_GOTO_HANDLE( ret );
	
	DestroyVulkanRendererDevice( vkRDev );
	return ret;
}

struct VulkanBudgetAllocator_T {
public:
	VulkanBudgetAllocator_T( size_t allocationBudget ) noexcept
		: budget( allocationBudget ) {}
	VkAllocationCallbacks GetCallbacks() const noexcept
	{
		return {
			.pUserData = (void *)this,
			.pfnAllocation = static_cast<PFN_vkAllocationFunction>( Allocate ),
			.pfnReallocation = static_cast<PFN_vkReallocationFunction>( Reallocate ),
			.pfnFree = static_cast<PFN_vkFreeFunction>( Free ),
			.pfnInternalAllocation = nullptr,
			.pfnInternalFree = nullptr,
		};
	}

	using ThisType = VulkanBudgetAllocator_T;

private:
	std::atomic<size_t> budget;

	static VKAPI_ATTR void *VKAPI_CALL Allocate(
		void *obj, size_t size, size_t alignment,
		VkSystemAllocationScope /* scope */ ) noexcept
	{
		auto &budget = static_cast<ThisType *>( obj )->budget;

		if( budget.fetch_sub( size, std::memory_order_relaxed ) >= size )
		{
			void *res = alignedAlloc( size, alignment );
			if( res ) return res;
		}

		// on fail return 'size' to budget
		budget.fetch_add( size, std::memory_order_relaxed );

		return nullptr;
	}

	static VKAPI_ATTR void *VKAPI_CALL Reallocate(
		void *obj, void *pOriginal, size_t size, size_t alignment,
		VkSystemAllocationScope scope ) noexcept
	{
		void *newAlloc = nullptr;
		if( size != 0 )
		{
			newAlloc = Allocate( obj, size, alignment, scope );
			if( !newAlloc ) return pOriginal;

			if( pOriginal )
			{
				size_t oldSize = GetAllocationHeader( pOriginal ).size;
				size_t commonSize = min( oldSize, size );

				memcpy( newAlloc, pOriginal, commonSize );
			}
		}

		Free( obj, pOriginal );
		return newAlloc;
	}

	static VKAPI_ATTR void VKAPI_CALL Free( void *obj, void *pMemory ) noexcept
	{
		auto &budget = static_cast<ThisType *>( obj )->budget;
		budget.fetch_add( alignedFree( pMemory ), std::memory_order_relaxed );
	}

	struct AllocationHeader {
		void *allocation;
		size_t size;
	};

	static AllocationHeader &GetAllocationHeader( void *memory )
	{
		return *( ( (AllocationHeader *)memory ) - 1 );
	}

	static void *alignedAlloc( size_t size, size_t alignment )
	{
		// make sure AllocationHeader is not overaligned
		static_assert( alignof( AllocationHeader ) <= alignof( max_align_t ) );

		alignment = max( alignment, alignof( AllocationHeader ) );
		size_t request_size = size + ( alignment + sizeof( AllocationHeader ) );
		char *buf = (char *)malloc( request_size );

		char *aligned = (char *)AlignUp( (uintptr_t)( buf + sizeof( AllocationHeader ) ), (uintptr_t)alignment );

		// store header just before returned ptr
		GetAllocationHeader( aligned ) = {
			.allocation = buf,
			.size = size
		};

		return (void *)aligned;
	}

	static size_t alignedFree( void *memory )
	{
		if( memory == nullptr ) return 0;

		AllocationHeader ah = GetAllocationHeader( memory );

		free( ah.allocation );
		return ah.size;
	}
};

constexpr bool VulkanBudgetAllocatorFitsInHandle = 
	sizeof( VulkanBudgetAllocator_T ) <= sizeof( VulkanBudgetAllocator );

VkResult CreateVulkanBudgetAllocator( VkDeviceSize size, VulkanBudgetAllocator *pBudgetAllocator )
{
	try
	{
		VulkanBudgetAllocator_T *dest;
		if constexpr( VulkanBudgetAllocatorFitsInHandle )
		{
			dest = reinterpret_cast<VulkanBudgetAllocator_T *>( pBudgetAllocator );
			new( dest ) VulkanBudgetAllocator_T( size );
		}
		else
		{
			dest = new VulkanBudgetAllocator_T( size );
			reinterpret_cast<VulkanBudgetAllocator_T *&>( *pBudgetAllocator ) = dest;
		}
	}
	catch( const std::bad_alloc & )
	{
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}

	return VK_SUCCESS;
}

void DestroyVulkanBudgetAllocator( VulkanBudgetAllocator budgetAllocator )
{
	if constexpr( !VulkanBudgetAllocatorFitsInHandle )
	{
		delete reinterpret_cast<VulkanBudgetAllocator_T *>(budgetAllocator);
	}
}

void GetVulkanBudgetCallbacks( VulkanBudgetAllocator budgetAllocator, VkAllocationCallbacks *pCallbacks )
{
	if constexpr( VulkanBudgetAllocatorFitsInHandle )
	{
		void *pAllocator = reinterpret_cast<void *>( &budgetAllocator );
		*pCallbacks = reinterpret_cast<VulkanBudgetAllocator_T *>( pAllocator )->GetCallbacks();
	}
	else
		*pCallbacks = reinterpret_cast<VulkanBudgetAllocator_T *>( budgetAllocator )->GetCallbacks();
}

// VkStructureType -> structure size, count
struct VulkanPNextChainManager_T
	: public std::unordered_map<VkStructureType, std::pair<VkDeviceSize, uint32_t>> {};

VkResult CreateVulkanPNextChainManager( VulkanPNextChainManager *pManager )
{
	try
	{
		*pManager = new VulkanPNextChainManager_T;
	}
	catch(const std::bad_alloc &)
	{
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}

	return VK_SUCCESS;
}

void DestroyVulkanPNextChainManager( VulkanPNextChainManager manager )
{
	if( manager ) delete manager;
}

VkResult AddVulkanPNextStructure( VulkanPNextChainManager manager, VkStructureType type, VkDeviceSize size )
{
	try
	{
		// I have no idea if structures can have size less aligned than
		// alignment requiremets for other structures so to ensure allocated
		// structure is aligned properly align all sizes to alignof( max_align_t )
		VkDeviceSize alignedSize = AlignUp( size, alignof( max_align_t ) );

		auto res = manager->try_emplace( type, alignedSize, 0 );

		// this is non throwing so 
		// if code enters here it always increment => no need to clean up
		res.first->second.second++; // increment count in structure info
	}
	catch( const std::bad_alloc & )
	{
		return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	}

	return VK_SUCCESS;
}

VkResult AllocateVulkanPNextChain( VulkanPNextChainManager manager, VkBaseOutStructure **ppNextChain )
{
	VkDeviceSize Size = 0;
	for( const auto &info : *manager )
		// Size += StructureSize * StructureCount
		Size += info.second.first * info.second.second;

	char *pNext = (char *)calloc( Size, 1 );
	if( !pNext ) return VK_ERROR_OUT_OF_DEVICE_MEMORY;

	VkDeviceSize offset = 0;

	VkBaseOutStructure *last = nullptr;

	for( const auto &info : *manager )
	{
		auto [type, SizeAndCount] = info;
		auto [size, count] = SizeAndCount;
		
		for( uint32_t i = 0; i < count; i++ )
		{
			last = (VkBaseOutStructure *)( pNext + offset );
			offset += size;

			last->sType = type;
			last->pNext = (VkBaseOutStructure *)( pNext + offset );
		}
	}

	if( last ) last->pNext = nullptr;

	*ppNextChain = reinterpret_cast<VkBaseOutStructure*>( pNext );
	return VK_SUCCESS;
}

void FreeVulkanPNextChain( VulkanPNextChainManager /* manager */, VkBaseOutStructure *pNextChain )
{
	if( pNextChain ) free( pNextChain );
}

void GetVulkanPNextChainStructureCount( VulkanPNextChainManager manager, VkStructureType type, uint32_t *pCount )
{
	auto idx = manager->find( type );
	*pCount = idx != manager->end() ? idx->second.second : 0;
}

VkResult CreateDepthResource(
	VulkanRenderDevice &vkDev, 
	uint32_t width, uint32_t height, 
	VulkanTexture *depth )
{
	*depth = {};
	VkResult ret = VK_SUCCESS;

	VkFormat depthFormat = FindDepthFormat( vkDev.device.physicalDevice );
	if( depthFormat == VK_FORMAT_UNDEFINED ) return VK_ERROR_INITIALIZATION_FAILED;

	ret = CreateImage( vkDev.device.allocator, depthFormat, VK_IMAGE_TYPE_2D,
		{ .width = width, .height = height, .depth = 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, VMA_MEMORY_USAGE_AUTO,
		&depth->image, &depth->imageMemory, nullptr );
	if( ret >= 0 )
	{
		ret = CreateImageView( vkDev.device.device, depth->image,
			depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, &depth->imageView );

		if( ret >= 0 )
		{
			VkCommandBuffer cmdBuffer;
			ret = BeginSingleTimeCommands( vkDev.device.device, vkDev.commandPool, &cmdBuffer );
			if( ret >= 0 )
			{
				TransitionImageLayoutCmd( cmdBuffer, depth->image, depthFormat,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
					VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
					VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT );

				ret = EndSingleTimeCommands( vkDev.device.device, vkDev.commandPool,
					vkDev.graphicsQueue.queue, cmdBuffer );
			}
		}
	}

	if( ret < 0 ) DestroyVulkanTexture( vkDev.device, *depth );

	return ret;
}

VkResult CreateTextureImage( VulkanRenderDevice &vkDev, 
	const char *filename, VulkanTexture *texture )
{
	*texture = {};
	VkResult ret = VK_SUCCESS;

	stbi_set_flip_vertically_on_load( true );

	int texWidth, texHeight, texChannels;
	stbi_uc *pixels = stbi_load( filename, 
		&texWidth, &texHeight, &texChannels, STBI_rgb_alpha );
	VkDeviceSize imageSize = texWidth * texHeight * 4;
	if( !pixels ) return VK_ERROR_INITIALIZATION_FAILED;

	VkBuffer stagingBuffer;
	VmaAllocation stagingMemory;
	ret = CreateBuffer( vkDev.device.allocator, imageSize, 0,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
		&stagingBuffer, &stagingMemory, nullptr );

	if( ret >= 0 )
	{
		void *pData;
		ret = vmaMapMemory( vkDev.device.allocator, stagingMemory, &pData );
		if( ret >= 0 )
		{
			memcpy( pData, pixels, imageSize );
			vmaUnmapMemory( vkDev.device.allocator, stagingMemory );
			ret = vmaFlushAllocation( vkDev.device.allocator, stagingMemory, 0, VK_WHOLE_SIZE );

			if( ret >= 0 )
			{
				const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
				const VkExtent3D imageExtent = { 
					.width = (uint32_t)texWidth, 
					.height = (uint32_t)texHeight,
					.depth = 1
				};

				ret = CreateImage( vkDev.device.allocator, imageFormat,
					VK_IMAGE_TYPE_2D, imageExtent,
					1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
					0, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					0, VMA_MEMORY_USAGE_AUTO,
					&texture->image, &texture->imageMemory, nullptr );

				if( ret >= 0 )
				{
					VkCommandBuffer cmdBuffer;
					ret = BeginSingleTimeCommands( vkDev.device.device, vkDev.commandPool, &cmdBuffer );
					if( ret >= 0 )
					{
						TransitionImageLayoutCmd( cmdBuffer, texture->image, imageFormat,
							VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
							VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
						CopyBufferToImageCmd( cmdBuffer, stagingBuffer, 0, texture->image,
							imageExtent, { 0, 0, 0 } );
						TransitionImageLayoutCmd( cmdBuffer, texture->image, imageFormat,
							VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
							VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT );

						ret = EndSingleTimeCommands( vkDev.device.device, vkDev.commandPool,
							vkDev.graphicsQueue.queue, cmdBuffer );
					}
				}
			}
		}

		vmaDestroyBuffer( vkDev.device.allocator, stagingBuffer, stagingMemory );
	}

	stbi_image_free( pixels );
	if( ret < 0 ) DestroyVulkanTexture( vkDev.device, *texture );

	return ret;
}

VkResult CreateSSBOVertexBuffer( 
	VulkanRenderDevice &vkDev, 
	const char *filename, 
	VkBuffer *storageBuffer, VmaAllocation *storageBufferMemory,
	VulkanBufferSuballocation *vertexBuffer,
	VulkanBufferSuballocation *indexBuffer )
{
	using namespace glm;
	VkResult res = VK_SUCCESS;

	const aiScene *scene = aiImportFile( filename, aiProcess_Triangulate );
	if( !scene || !scene->HasMeshes() ) return VK_ERROR_INITIALIZATION_FAILED;

	const aiMesh *mesh = scene->mMeshes[ 0 ];
	struct VertexData {
		vec3 pos;
		vec2 uv;
	};
	vertexBuffer->size = sizeof( VertexData ) * mesh->mNumVertices;
	indexBuffer->size = sizeof( uint32_t ) * mesh->mNumFaces * 3;

	const VkPhysicalDeviceProperties *properties;
	vmaGetPhysicalDeviceProperties( vkDev.device.allocator, &properties );
	
	vertexBuffer->offset = 0;
	indexBuffer->offset = AlignUp( vertexBuffer->size, properties->limits.minStorageBufferOffsetAlignment );

	VkDeviceSize bufferSize = indexBuffer->offset + indexBuffer->size;

	res = CreateBuffer( vkDev.device.allocator, bufferSize, 0,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		0,
		VMA_MEMORY_USAGE_AUTO,
		storageBuffer, storageBufferMemory, nullptr );
	if( res >= 0 )
	{
		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VmaAllocation stagingBufferMemory = VK_NULL_HANDLE;

		res = CreateBuffer( vkDev.device.allocator, bufferSize, 0,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			VMA_MEMORY_USAGE_AUTO,
			&stagingBuffer, &stagingBufferMemory, nullptr );

		if( res >= 0 )
		{
			void *pData;
			res = vmaMapMemory( vkDev.device.allocator, stagingBufferMemory, &pData );
			if( res >= 0 )
			{
				VertexData *vertexData = (VertexData *)( (char *)pData + vertexBuffer->offset );
				for( uint32_t i = 0; i != mesh->mNumVertices; i++ )
				{
					const aiVector3D v = mesh->mVertices[ i ];
					const aiVector3D t = mesh->mTextureCoords[ 0 ][ i ];
					*vertexData++ = VertexData{
						.pos = vec3( v.x, v.y, v.z ),
						.uv = vec2( t.x, t.y )
					};
				}

				uint32_t *indexData = (uint32_t *)( (char *)pData + indexBuffer->offset );
				for( uint32_t i = 0; i != mesh->mNumFaces; i++ )
				{
					assert( mesh->mFaces[ i ].mNumIndices == 3 );
					for( uint32_t j = 0; j != 3; j++ )
						*indexData++ = mesh->mFaces[ i ].mIndices[ j ];
				}

				vmaUnmapMemory( vkDev.device.allocator, stagingBufferMemory );
				res = vmaFlushAllocation( vkDev.device.allocator, stagingBufferMemory, 0, VK_WHOLE_SIZE );
			}

			if( res >= 0 )
			{
				VkCommandBuffer cmdBuffer;
				res = BeginSingleTimeCommands( vkDev.device.device, vkDev.commandPool, &cmdBuffer );
				if( res >= 0 )
				{
					const VkBufferCopy copyParam = {
						.srcOffset = 0,
						.dstOffset = 0,
						.size = bufferSize
					};

					vkCmdCopyBuffer( cmdBuffer, stagingBuffer, *storageBuffer, 1, &copyParam );

					res = EndSingleTimeCommands(
						vkDev.device.device, vkDev.commandPool, vkDev.graphicsQueue.queue, cmdBuffer );
				}
			}

			vmaDestroyBuffer( vkDev.device.allocator, stagingBuffer, stagingBufferMemory );
		}

		if( res < 0 )
		{
			vmaDestroyBuffer( vkDev.device.allocator, *storageBuffer, *storageBufferMemory );
			*storageBuffer = VK_NULL_HANDLE;
			*storageBufferMemory = VK_NULL_HANDLE;
		}
	}

	return res;
}

VkResult CreateEngineDescriptorSetLayout( VkDevice device, VkDescriptorSetLayout *layout )
{
	constexpr auto DescriptorSetBinding = [](
		uint32_t binding, 
		VkDescriptorType type, 
		VkPipelineStageFlags stages ) -> VkDescriptorSetLayoutBinding
	{
		return {
			.binding = binding,
			.descriptorType = type,
			.descriptorCount = 1,
			.stageFlags = stages,
			.pImmutableSamplers = nullptr
		};
	};

	const VkDescriptorSetLayoutBinding bindings[] = {
		DescriptorSetBinding( 0, 
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
			VK_SHADER_STAGE_VERTEX_BIT ),
		DescriptorSetBinding( 1,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT ),
		DescriptorSetBinding( 2,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT ),
		DescriptorSetBinding( 3,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT ),
	};

	const VkDescriptorSetLayoutCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = (uint32_t)size( bindings ),
		.pBindings = bindings
	};

	return vkCreateDescriptorSetLayout( device, &ci, nullptr, layout );
}

VkResult CreateEngineDescriptorSets( VulkanRenderDevice &vkDev, VulkanState &vkState )
{
	VkResult res = VK_SUCCESS;

	std::vector<VkDescriptorSetLayout> layouts( 
		vkDev.swapchainImages.size(), 
		vkState.descriptorSetLayout
	);

	const VkDescriptorSetAllocateInfo ai = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = vkState.descriptorPool,
		.descriptorSetCount = (uint32_t)layouts.size(),
		.pSetLayouts = layouts.data()
	};

	vkState.descriptorSets.resize( layouts.size() );

	res = vkAllocateDescriptorSets( vkDev.device.device, &ai, vkState.descriptorSets.data() );

	if( res >= 0 )
	{
		std::vector<VkWriteDescriptorSet> writes;

		for( size_t i = 0; i < layouts.size(); i++ )
		{
			const VkDescriptorBufferInfo uniformBufferInfo = {
				.buffer = vkState.uniformBufferMemory.buffer,
				.offset = vkState.uniformBuffers[ i ].offset,
				.range = vkState.uniformBuffers[ i ].size
			};

			const VkDescriptorBufferInfo vertexBufferInfo = {
				.buffer = vkState.modelBuffer.buffer,
				.offset = vkState.vertexBuffer.offset,
				.range = vkState.vertexBuffer.size
			};

			const VkDescriptorBufferInfo indexBufferInfo = {
				.buffer = vkState.modelBuffer.buffer,
				.offset = vkState.indexBuffer.offset,
				.range = vkState.indexBuffer.size
			};

			const VkDescriptorImageInfo textureInfo = {
				.sampler = vkState.textureSampler,
				.imageView = vkState.texture.imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};

			auto WriteDescriptorSet = [ & ](
				uint32_t binding,
				VkDescriptorType type,
				const VkDescriptorImageInfo *ii,
				const VkDescriptorBufferInfo *bi,
				const VkBufferView *bv
				)
			{
				writes.push_back( VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.pNext = nullptr,
					.dstSet = vkState.descriptorSets[ i ],
					.dstBinding = binding,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = type,
					.pImageInfo = ii,
					.pBufferInfo = bi,
					.pTexelBufferView = bv
					} );
			};

			WriteDescriptorSet( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &uniformBufferInfo, nullptr );
			WriteDescriptorSet( 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &vertexBufferInfo, nullptr );
			WriteDescriptorSet( 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &indexBufferInfo, nullptr );
			WriteDescriptorSet( 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textureInfo, nullptr, nullptr );
		}

		vkUpdateDescriptorSets( vkDev.device.device, (uint32_t)writes.size(), writes.data(), 0, nullptr );
	}

	return res;
}

void DestroyVulkanState( VulkanDevice &vkDev, VulkanState &vkState )
{
	vkDestroyDescriptorPool( vkDev.device, vkState.descriptorPool, nullptr );
	vkDestroyDescriptorSetLayout( vkDev.device, vkState.descriptorSetLayout, nullptr );

	vmaDestroyBuffer( vkDev.allocator, vkState.uniformBufferMemory.buffer,
		vkState.uniformBufferMemory.bufferAllocation );
	
	vmaDestroyBuffer( vkDev.allocator, vkState.modelBuffer.buffer,
		vkState.modelBuffer.bufferAllocation );
	
	vkDestroySampler( vkDev.device, vkState.textureSampler, nullptr );
	DestroyVulkanTexture( vkDev, vkState.texture );

	vkDestroyPipelineLayout( vkDev.device, vkState.layout, nullptr );
	vkDestroyPipeline( vkDev.device, vkState.graphicsPipeline, nullptr );

	DestroyVulkanTexture( vkDev, vkState.depthResource ); 

	vkState = VulkanState{};
}


void DestroyVulkanRendererDevice( VulkanRenderDevice &vkDev )
{
	for( size_t i = 0; i < vkDev.swapchainImageViews.size(); i++ )
		vkDestroyImageView( vkDev.device.device, vkDev.swapchainImageViews[ i ], nullptr );
	
	vkDestroySwapchainKHR( vkDev.device.device, vkDev.swapchain, nullptr );
	vkDestroyCommandPool( vkDev.device.device, vkDev.commandPool, nullptr );
	vkDestroySemaphore( vkDev.device.device, vkDev.semophore, nullptr );
	vkDestroySemaphore( vkDev.device.device, vkDev.renderSemaphore, nullptr );

	vkDev = VulkanRenderDevice{};
}

void DestroyVulkanDevice( VulkanDevice &vkDev )
{
	vmaDestroyAllocator( vkDev.allocator );
	vkDestroyDevice( vkDev.device, nullptr );

	vkDev = VulkanDevice{};
}

void DestroyVulkanInstance( VulkanInstance &vk )
{
	vkDestroySurfaceKHR( vk.instance, vk.surface, nullptr );
	if( vk.reportCallback ) vkDestroyDebugReportCallbackEXT( vk.instance, vk.reportCallback, nullptr );
	if( vk.messenger ) vkDestroyDebugUtilsMessengerEXT( vk.instance, vk.messenger, nullptr );
	vkDestroyInstance( vk.instance, nullptr );

	vk = VulkanInstance{};
}

void DestroyVulkanTexture( const VulkanDevice &device, VulkanTexture &texture )
{
	vkDestroyImageView( device.device, texture.imageView, nullptr );
	vmaDestroyImage( device.allocator, texture.image, texture.imageMemory );

	texture = VulkanTexture{};
}

VkResult BeginSingleTimeCommands( VkDevice device, VkCommandPool commandPool, VkCommandBuffer *commandBuffer )
{
	VkResult ret = VK_SUCCESS;
	*commandBuffer = VK_NULL_HANDLE;

	const VkCommandBufferAllocateInfo ai = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	ret = vkAllocateCommandBuffers( device, &ai, commandBuffer );
	if( ret >= 0 )
	{
		const VkCommandBufferBeginInfo beginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr
		};

		ret = vkBeginCommandBuffer( *commandBuffer, &beginInfo );

		if( ret >= 0 ) return VK_SUCCESS;
	}

	vkFreeCommandBuffers( device, commandPool, 1, commandBuffer );
	*commandBuffer = VK_NULL_HANDLE;
	return ret;
}

VkResult EndSingleTimeCommands( VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer )
{
	VkResult ret = VK_SUCCESS;

	ret = vkEndCommandBuffer( commandBuffer );
	if( ret >= 0 )
	{
		const VkFenceCreateInfo fci = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0
		};

		VkFence fence;

		ret = vkCreateFence( device, &fci, nullptr, &fence );
		if( ret >= 0 )
		{
			const VkSubmitInfo si = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.pNext = nullptr,
				.waitSemaphoreCount = 0,
				.pWaitSemaphores = nullptr,
				.pWaitDstStageMask = nullptr,
				.commandBufferCount = 1,
				.pCommandBuffers = &commandBuffer,
				.signalSemaphoreCount = 0,
				.pSignalSemaphores = nullptr
			};

			ret = vkQueueSubmit( queue, 1, &si, fence );
			if( ret >= 0 ) ret = vkWaitForFences( device, 1, &fence, VK_TRUE, UINT32_MAX );

			vkDestroyFence( device, fence, nullptr );
		}
	}
	vkFreeCommandBuffers( device, commandPool, 1, &commandBuffer );
	return ret;
}



VkResult RemoveUnsupportedLayers(std::vector<const char*>& layers)
{
	std::vector<VkLayerProperties> avaiableLayers;
	VK_CHECK_RET( get_vector( avaiableLayers, vkEnumerateInstanceLayerProperties ) );
	for (auto layer = layers.begin(); layer < layers.end(); )
	{
		if (!std::any_of(avaiableLayers.begin(), avaiableLayers.end(),
			[layer](VkLayerProperties& l) { return strcmp(*layer, l.layerName) == 0; }))
			layer = layers.erase(layer);
		else layer++;
	}

	return VK_SUCCESS;
}

VkResult RemoveUnsupportedExtensions(const std::vector<const char*> &layers, std::vector<const char*>& exts)
{
	std::vector<std::vector<VkExtensionProperties>> avaiableExtensions(layers.size() + 1);
	VK_CHECK_RET( get_vector( avaiableExtensions[ 0 ], vkEnumerateInstanceExtensionProperties, nullptr ) );
	for (int i = 0; i < (int)layers.size(); i++)
		VK_CHECK_RET( get_vector( avaiableExtensions[ i + 1 ], vkEnumerateInstanceExtensionProperties, layers[i] ));

	for (auto ext = exts.begin(); ext < exts.end(); )
	{
		if (
			!std::any_of(avaiableExtensions.begin(), avaiableExtensions.end(),
				[ext](std::vector<VkExtensionProperties>& avaiableExtsForLayer)
				{
					return std::any_of(avaiableExtsForLayer.begin(), avaiableExtsForLayer.end(),
						[ext](VkExtensionProperties& l) { return strcmp(*ext, l.extensionName) == 0; });
				})
			)
			ext = exts.erase(ext);
		else ext++;
	}

	return VK_SUCCESS;
}

VkResult CreateInstance(
	const std::vector<const char *> &layers,
	const std::vector<const char *> &extensions,
	const VkApplicationInfo *pAppInfo,
	bool enumeratePortability,
	void *pCreateInstanceNext,
	VkInstance* pInstance )
{
	uint32_t SurfaceExtCnt;
	const char **surfaceExts = glfwGetRequiredInstanceExtensions(&SurfaceExtCnt);

	std::vector<const char *> exts = extensions;
	for (uint32_t i = 0; i < SurfaceExtCnt; i++) 
		if( exts.end() == std::find( exts.begin(), exts.end(), std::string_view(surfaceExts[i]) ) )
			exts.push_back( surfaceExts[i] );

	uint32_t maxVer = volkGetInstanceVersion();
	if (maxVer == 0) return VK_ERROR_INITIALIZATION_FAILED;

	// we do not support other variants of Vulkan
	if ( VK_API_VERSION_VARIANT(maxVer) != 0 ) return VK_ERROR_INCOMPATIBLE_DRIVER;

	bool Vulkan1_0 = 
		VK_API_VERSION_MAJOR(maxVer) == 1 && 
		VK_API_VERSION_MINOR(maxVer) == 0;

	VkApplicationInfo appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = NULL,
		.applicationVersion = 0,
		.pEngineName = "4dGraphics",
		.engineVersion = VK_MAKE_API_VERSION(0,1,0,0),
		.apiVersion = Vulkan1_0 ? VK_API_VERSION_1_0 : VK_API_VERSION_1_3
	};

	if( pAppInfo ) // transfer all data that user wants
	{
		appInfo.pNext = pAppInfo->pNext;
		appInfo.pApplicationName = pAppInfo->pApplicationName;
		appInfo.applicationVersion = pAppInfo->applicationVersion;
		if( !Vulkan1_0 )
			appInfo.apiVersion = max( appInfo.apiVersion, pAppInfo->apiVersion );
	}
	
	VkInstanceCreateFlags flags = 0;

	if( enumeratePortability && 
		exts.end() != std::find( exts.begin(), exts.end(),
			std::string_view( VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) ) )
		flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	
	VkInstanceCreateInfo InstanceCI{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = pCreateInstanceNext,
			.flags = flags,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = (uint32_t)layers.size(),
			.ppEnabledLayerNames = layers.data(),
			.enabledExtensionCount = (uint32_t)exts.size(),
			.ppEnabledExtensionNames = exts.data()
	};

	VK_CHECK_RET( vkCreateInstance(&InstanceCI, nullptr, pInstance) );

	volkLoadInstance(*pInstance);
	return VK_SUCCESS;
}

VkResult CreateDevice( VkPhysicalDevice physicalDevice,
	const std::vector<VkDeviceQueueCreateInfo> &families,
	const std::vector<const char *> &extensions,
	const VkPhysicalDeviceFeatures2 *deviceFeatures, VkDevice *pDevice )
{
	VkDeviceCreateInfo dCI = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = deviceFeatures,
		.flags = 0,
		.queueCreateInfoCount = (uint32_t)families.size(),
		.pQueueCreateInfos = families.data(),
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = (uint32_t)extensions.size(),
		.ppEnabledExtensionNames = extensions.data(),
		.pEnabledFeatures = nullptr
	};

	return vkCreateDevice(physicalDevice, &dCI, nullptr, pDevice);
}

VkResult FindSuitablePhysicalDevice(VkInstance instance, std::function<bool(VkPhysicalDevice)> selector, VkPhysicalDevice* physicalDevice)
{
	std::vector<VkPhysicalDevice> physicalDevices;
	VK_CHECK_RET(get_vector( physicalDevices, vkEnumeratePhysicalDevices, instance ));

	for (VkPhysicalDevice dev : physicalDevices)
	{
		if (selector(dev))
		{
			*physicalDevice = dev;
			return VK_SUCCESS;
		}
	}

	return VK_ERROR_INITIALIZATION_FAILED;
}

uint32_t FindQueueFamilies(VkPhysicalDevice device, VkQueueFlags desiredFlags)
{
	auto families = get_vector_noerror<VkQueueFamilyProperties>( vkGetPhysicalDeviceQueueFamilyProperties, device );

	for( uint32_t i = 0; i < (uint32_t)families.size(); i++ )
	{
		if (families[i].queueCount > 0 && (families[i].queueFlags & desiredFlags) == desiredFlags )
			return i;
	}

	return UINT32_MAX;
}

VkResult SetupDebugCallbacks( VkInstance instance,
	VkDebugUtilsMessengerEXT *messenger, PFN_vkDebugUtilsMessengerCallbackEXT messengerCallback, void *messengerUserData, 
	VkDebugReportCallbackEXT *reportCallback, PFN_vkDebugReportCallbackEXT reportMessageCallback, void *reportUserData )
{
	*messenger = VK_NULL_HANDLE;
	*reportCallback = VK_NULL_HANDLE;

	const VkDebugUtilsMessengerCreateInfoEXT MessCI = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.pNext = nullptr,
		.flags = 0,
		.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
		.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
		.pfnUserCallback = messengerCallback,
		.pUserData = messengerUserData
	};

	VK_CHECK_RET( vkCreateDebugUtilsMessengerEXT( instance, &MessCI, nullptr, messenger ) );

	const VkDebugReportCallbackCreateInfoEXT RepCI = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
		.pNext = nullptr,
		.flags =
			VK_DEBUG_REPORT_ERROR_BIT_EXT |
			VK_DEBUG_REPORT_WARNING_BIT_EXT |
			VK_DEBUG_REPORT_DEBUG_BIT_EXT |
			VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,
		.pfnCallback = reportMessageCallback,
		.pUserData = reportUserData
	};

	VkResult res = vkCreateDebugReportCallbackEXT( instance, &RepCI, nullptr, reportCallback );
	if( res < 0 ) 
	{ 
		VK_ASSERT( false );
		vkDestroyDebugUtilsMessengerEXT( instance, *messenger, nullptr ); 
		*messenger = VK_NULL_HANDLE; 
		return res;
	}

	return VK_SUCCESS;
}

VkResult SetVkObjectName( VkDevice device, uint64_t object, VkObjectType objType, const char *name )
{
	VkDebugUtilsObjectNameInfoEXT nameInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = objType,
		.objectHandle = object,
		.pObjectName = name
	};

	return vkSetDebugUtilsObjectNameEXT( device, &nameInfo );
}

VkResult QuerySwapchainSupport( VkPhysicalDevice device, VkSurfaceKHR surface, SwapchainSupportDetails *details )
{
	VK_CHECK_RET( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, surface, &details->capabilities ) );
	VK_CHECK_RET( get_vector( details->formats, vkGetPhysicalDeviceSurfaceFormatsKHR, device, surface ) );
	VK_CHECK_RET( get_vector( details->presentModes, vkGetPhysicalDeviceSurfacePresentModesKHR, device, surface ) );

	return VK_SUCCESS;
}

VkSurfaceFormatKHR ChooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR> &avaiableFormats )
{
	(void)avaiableFormats;
	return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
}

VkPresentModeKHR ChooseSwapPresentMode( const std::vector<VkPresentModeKHR> &avaiablePresentModes )
{
	for( const auto mode : avaiablePresentModes )
		if( mode == VK_PRESENT_MODE_MAILBOX_KHR )
			return mode;

	return VK_PRESENT_MODE_FIFO_KHR;
}

uint32_t ChooseSwapImageCount( const VkSurfaceCapabilitiesKHR &capabilities )
{
	const uint32_t wantedImageCount = capabilities.minImageCount + 1;

	return
		capabilities.maxImageCount == 0 ?	// 0 means that there is no maximum
		wantedImageCount :
		max( wantedImageCount, capabilities.maxImageCount );
}

VkResult CreateSwapchain( VkDevice device, VkPhysicalDevice physicalDevice, uint32_t graphicsFamily, VkSurfaceKHR surface, uint32_t width, uint32_t height, VkSwapchainKHR *swapchain )
{
	SwapchainSupportDetails swapchainSupport;

	VK_CHECK_RET( QuerySwapchainSupport( physicalDevice, surface, &swapchainSupport ) );
	VkSurfaceFormatKHR format = ChooseSwapSurfaceFormat( swapchainSupport.formats );
	VkPresentModeKHR presentMode = ChooseSwapPresentMode( swapchainSupport.presentModes );

	const VkSwapchainCreateInfoKHR swapCI = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = surface,
		.minImageCount = ChooseSwapImageCount( swapchainSupport.capabilities ),
		.imageFormat = format.format,
		.imageColorSpace = format.colorSpace,
		.imageExtent = {.width = width, .height = height },
		.imageArrayLayers = 1, // for stereoscopic 3d this should be 2
		.imageUsage =
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &graphicsFamily,
		.preTransform = swapchainSupport.capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = presentMode,
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE
	};

	return vkCreateSwapchainKHR( device, &swapCI, nullptr, swapchain );
}

VkResult CreateSwapchainImages( VkDevice device, VkSwapchainKHR swapchain, uint32_t *swapchainImageCount, std::vector<VkImage> &swapchainImages, std::vector<VkImageView> &swapchainImageViews )
{
	VK_CHECK_RET( vkGetSwapchainImagesKHR( device, swapchain, swapchainImageCount, nullptr ) );
	swapchainImages.resize( *swapchainImageCount );
	swapchainImageViews.resize( *swapchainImageCount );
	VK_CHECK_RET( vkGetSwapchainImagesKHR( device, swapchain, swapchainImageCount, swapchainImages.data() ) );

	for( uint32_t i = 0; i < *swapchainImageCount; i++ )
	{
		VkResult res = CreateImageView( device, swapchainImages[ i ], VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, &swapchainImageViews[ i ] );
		if( res < 0 ) // error something went wrong
		{ 
			VK_ASSERT( false );
			while( i != 0 ) 
				vkDestroyImageView( device, swapchainImageViews[ --i ], nullptr );
			swapchainImages.clear();
			swapchainImageViews.clear();
			return res;
		}
	}

	return VK_SUCCESS;
}


void GetSuballocatedBufferSize( 
	const std::vector<VkDeviceSize> &sizes,
	VkDeviceSize alignment, 
	VkDeviceSize *bufferSize,
	std::vector<VulkanBufferSuballocation> &suballocations )
{
	for( auto size : sizes )
	{
		suballocations.push_back({
			.offset = AlignUp( *bufferSize, alignment ),
			.size = size
		});
		
		*bufferSize = suballocations.back().offset + size;
	}
}

VkResult CreateBuffer( 
	VmaAllocator allocator,
	VkDeviceSize size, VkBufferCreateFlags flags, VkBufferUsageFlags usage,
	VmaAllocationCreateFlags allocationFlags, VmaMemoryUsage vmaUsage,
	VkBuffer *buffer, VmaAllocation *bufferMemory, VmaAllocationInfo *allocationInfo )
{
	const VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr
	};

	VmaAllocationCreateInfo ai{};

	ai.flags = allocationFlags;
	ai.usage = vmaUsage;

	return vmaCreateBuffer( allocator, &bufferInfo, &ai, buffer, bufferMemory, allocationInfo );
}

VkResult CreateImage( 
	VmaAllocator allocator, 
	VkFormat format, VkImageType imageType, VkExtent3D size, uint32_t mipLevels, uint32_t arrayLayers, VkSampleCountFlagBits samples,
	VkImageTiling tiling, VkImageCreateFlags flags, VkImageUsageFlags usage, 
	VmaAllocationCreateFlags allocationFlags, VmaMemoryUsage vmaUsage, 
	VkImage *image, VmaAllocation *imageMemory, VmaAllocationInfo *allocationInfo )
{
	const VkImageCreateInfo ici = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags,
		.imageType = imageType,
		.format = format,
		.extent = size,
		.mipLevels = mipLevels,
		.arrayLayers = arrayLayers,
		.samples = samples,
		.tiling = tiling,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	
	VmaAllocationCreateInfo ai{};

	ai.flags = allocationFlags;
	ai.usage = vmaUsage;

	return vmaCreateImage(allocator, &ici, &ai, image, imageMemory, allocationInfo );
}

VkResult CreateImageView( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView *imageView )
{
	const VkImageViewCreateInfo viewCI = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.components = {
			.r = VK_COMPONENT_SWIZZLE_R,
			.g = VK_COMPONENT_SWIZZLE_G,
			.b = VK_COMPONENT_SWIZZLE_B,
			.a = VK_COMPONENT_SWIZZLE_A,
		},	
		.subresourceRange = {
			.aspectMask = aspectFlags,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	return vkCreateImageView( device, &viewCI, nullptr, imageView );
}

VkResult CreateTextureSampler( VkDevice device, 
	VkFilter filter, VkSamplerMipmapMode mipMode,
	VkSamplerAddressMode addressU, VkSamplerAddressMode addressV, VkSamplerAddressMode addressW, 
	float anisotropy,
	VkSampler *sampler )
{
	const VkSamplerCreateInfo sci = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = filter,
		.minFilter = filter,
		.mipmapMode = mipMode,
		.addressModeU = addressU,
		.addressModeV = addressV,
		.addressModeW = addressW,
		.mipLodBias = 0.0f,
		.anisotropyEnable = anisotropy != 1.0f ? VK_TRUE : VK_FALSE,
		.maxAnisotropy = anisotropy,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f, .maxLod = VK_LOD_CLAMP_NONE,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE
	};

	return vkCreateSampler( device, &sci, nullptr, sampler );
}

void CopyBufferToImageCmd( 
	VkCommandBuffer cmdBuffer,
	VkBuffer srcBuffer, VkDeviceSize srcOffset, 
	VkImage dstImage, VkExtent3D size, VkOffset3D offset )
{
	const VkBufferImageCopy region = {
		.bufferOffset = srcOffset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageOffset = offset,
		.imageExtent = size
	};

	vkCmdCopyBufferToImage( cmdBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
}

void TransitionImageLayoutCmd(
	VkCommandBuffer cmdBuffer,
	VkImage image, VkFormat format,
	VkImageLayout oldLayout, VkImageLayout newLayout,
	VkPipelineStageFlags srcStageMask, VkAccessFlags srcAccessMask,
	VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask )
{
	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = srcAccessMask,
		.dstAccessMask = dstAccessMask,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = {
			.aspectMask = 0,
			.baseMipLevel = 0,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = VK_REMAINING_ARRAY_LAYERS
		}
	};

	bool depth = FormatHasDepthComponent( format ), stencil = FormatHasStencilComponent( format );
	if( depth ) barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
	if( stencil ) barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	if( !depth && !stencil ) barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;

	vkCmdPipelineBarrier( cmdBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier );
}

VkFormat FindSupportedFormat( 
	VkPhysicalDevice device, 
	const std::vector<VkFormat> &candidates,
	VkImageTiling tiling, VkFormatFeatureFlags features )
{
	bool lin = tiling == VK_IMAGE_TILING_LINEAR;
	bool opt = tiling == VK_IMAGE_TILING_OPTIMAL;

	if( !lin && !opt ) return VK_FORMAT_UNDEFINED;

	for( VkFormat fmt : candidates )
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties( device, fmt, &properties );
		if( ( ( lin ? properties.linearTilingFeatures :
				opt ? properties.optimalTilingFeatures :
				( assert( 0 ), 0 ) ) & features ) == features 
			) return fmt;
	}

	return VK_FORMAT_UNDEFINED;
}

VkFormat FindDepthFormat( VkPhysicalDevice device )
{
	return FindSupportedFormat( device, 
		{
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_X8_D24_UNORM_PACK32,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
		},
		VK_IMAGE_TILING_OPTIMAL, 
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
}

bool FormatHasDepthComponent( VkFormat fmt )
{
	const VkFormat depthFormats[] = { 
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_D16_UNORM_S8_UINT
	};

	for( VkFormat d : depthFormats ) if( d == fmt ) return true;
	return false;
}

bool FormatHasStencilComponent( VkFormat fmt )
{
	const VkFormat stencilFormats[] = {
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_S8_UINT
	};

	for( VkFormat s : stencilFormats ) if( s == fmt ) return true;
	return false;
}

VkResult CreateSemophore( VkDevice device, VkSemaphore *semaphore )
{
	const VkSemaphoreCreateInfo sCI = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0
	};
	return vkCreateSemaphore( device, &sCI, nullptr, semaphore );
}

VkResult CreateTimelineSemaphore( VkDevice dev, uint64_t initialValue, VkSemaphore *semaphore )
{
	const VkSemaphoreTypeCreateInfo stci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext = nullptr,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = initialValue
	};
	const VkSemaphoreCreateInfo sCI = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &stci,
		.flags = 0
	};

	return vkCreateSemaphore( dev, &sCI, nullptr, semaphore );
}

VkResult CreateDescriptorPool(
	VkDevice device,
	void *pNext,
	VkDescriptorPoolCreateFlags flags,
	uint32_t maxSets,
	uint32_t poolSizeCount,
	const VkDescriptorPoolSize *pPoolSizes,
	VkDescriptorPool *descPool )
{
	const VkDescriptorPoolCreateInfo pi = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = pNext,
		.flags = flags,
		.maxSets = maxSets,
		.poolSizeCount = poolSizeCount,
		.pPoolSizes = pPoolSizes
	};
	return vkCreateDescriptorPool( device, &pi, nullptr, descPool );
}

VkResult CreateDescriptorSetHelper( 
	VkDevice device, 
	VkDescriptorPoolCreateFlags flags,
	uint32_t maxSets, 
	uint32_t uniformBuffersPerSet,
	uint32_t storageBuffersPerSet, 
	uint32_t imageSamplersPerSet, 
	VkDescriptorPool *descPool )
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	if( uniformBuffersPerSet )
		poolSizes.push_back( {
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			uniformBuffersPerSet * maxSets
		} );

	if( storageBuffersPerSet )
		poolSizes.push_back( {
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			storageBuffersPerSet * maxSets
		} );

	if( imageSamplersPerSet )
		poolSizes.push_back( {
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			imageSamplersPerSet * maxSets
		} );

	return CreateDescriptorPool(
		device, nullptr, flags,
		maxSets, 
		(uint32_t)poolSizes.size(), poolSizes.data(),
		descPool
	);
}

VkResult CreateShaderModule( 
	VkDevice device, 
	const char *filename, 
	VkShaderModuleCreateFlags flags,
	const void *pNext,
	VkShaderModule *shaderModule )
{
	auto stage = stageFromFilename(filename);
	if( stage == EShLangCount ) return VK_ERROR_INITIALIZATION_FAILED;

	std::vector<uint32_t> SPIRV = getShaderOrGenerate( stage, filename );
	if( SPIRV.empty() ) return VK_ERROR_INITIALIZATION_FAILED;

	const VkShaderModuleCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = pNext,
		.flags = flags,
		.codeSize = (uint32_t)SPIRV.size() * sizeof(uint32_t),
		.pCode = SPIRV.data()
	};

	return vkCreateShaderModule( device, &ci, nullptr, shaderModule );
}

VkResult CreatePipelineLayout( 
	VkDevice device, 
	uint32_t setLayoutCount, const VkDescriptorSetLayout *pSetLayouts, 
	uint32_t pushConstantRangeCount, const VkPushConstantRange *pPushConstantRanges, 
	VkPipelineLayout *layout )
{
	const VkPipelineLayoutCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = setLayoutCount,
		.pSetLayouts = pSetLayouts,
		.pushConstantRangeCount = pushConstantRangeCount,
		.pPushConstantRanges = pPushConstantRanges
	};

	return vkCreatePipelineLayout( device, &ci, nullptr, layout );
}


	constexpr VkPipelineVertexInputStateCreateInfo vici = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr
	};

	constexpr VkPipelineInputAssemblyStateCreateInfo iaci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE
	};

	constexpr VkPipelineTessellationStateCreateInfo tsci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.patchControlPoints = 0,
	};

	constexpr VkPipelineViewportStateCreateInfo vsci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr,
	};

	constexpr VkPipelineRasterizationStateCreateInfo rsci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f
	};

	constexpr VkPipelineMultisampleStateCreateInfo msci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE
	};

	constexpr VkPipelineDepthStencilStateCreateInfo dssci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
		.depthBoundsTestEnable = VK_TRUE,
		.stencilTestEnable = VK_FALSE,
		.front = {},
		.back = {},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f,
	};

	constexpr VkPipelineColorBlendAttachmentState attachmentBlend = {
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = 
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT
	};

	constexpr VkPipelineColorBlendStateCreateInfo cbsci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_NO_OP,
		.attachmentCount = 1,
		.pAttachments = &attachmentBlend,
		.blendConstants = { 0.0f, 0.0f, 0.0f, 1.0f }
	};

void FillGraphicsPipelineDefaults( VkGraphicsPipelineCreateInfo *gpci )
{
	*gpci = VkGraphicsPipelineCreateInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stageCount = 0,
		.pStages = nullptr,
		.pVertexInputState = &vici,
		.pInputAssemblyState = &iaci,
		.pTessellationState = &tsci,
		.pViewportState = &vsci,
		.pRasterizationState = &rsci,
		.pMultisampleState = &msci,
		.pDepthStencilState = &dssci,
		.pColorBlendState = &cbsci,
		.pDynamicState = nullptr,
		.layout = VK_NULL_HANDLE,
		.renderPass = VK_NULL_HANDLE,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};
}

VkPipelineShaderStageCreateInfo FillShaderStage( 
	VkShaderStageFlagBits stage, 
	VkShaderModule module, const char *name )
{
	return VkPipelineShaderStageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = stage,
		.module = module,
		.pName = name,
		.pSpecializationInfo = nullptr
	};
}