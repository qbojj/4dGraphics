#include "GameRenderHandler.h"
#include "GameTickHandler.h"

#include "Debug.h"
#include "VulkanHelpers.h"
#include <CommonUtility.h>

using namespace std;

#define ASSERT_LOG_RETURN( expr, logmsg, ... ) do{	\
	if( !(expr) )									\
	{												\
		TRACE(DebugLevel::FatalError,				\
			logmsg __VA_OPT__(, ) __VA_ARGS__);		\
		return false;								\
	}												\
}while(0)

#define CHECK_LOG_RETURN( expr, logmsg, ... ) do{	\
	VkResult result_check_log_ = (expr);			\
	if( result_check_log_ < 0 )						\
	{												\
		TRACE(DebugLevel::FatalError,				\
			logmsg " (cause: %s)\n" __VA_OPT__(, ) __VA_ARGS__,	\
			VulkanResultErrorCause(result_check_log_));			\
		return false;								\
	}												\
}while(0)

#define CHECK_LOG_RETURN_NOVAL( expr, logmsg, ... ) do{	\
	VkResult result_check_log_ = (expr);			\
	if( result_check_log_ < 0 )						\
	{												\
		TRACE(DebugLevel::FatalError,				\
			logmsg " (cause: %s)\n" __VA_OPT__(, ) __VA_ARGS__,	\
			VulkanResultErrorCause(result_check_log_));			\
		return ;									\
	}												\
}while(0)

static VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
	VkDebugUtilsMessageTypeFlagsEXT Type,
	const VkDebugUtilsMessengerCallbackDataEXT * CallbackData,
	void *UserData )
{
	(void)UserData;
	(void)Type;

	DebugLevel lev = DebugLevel::Debug;
	if( Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT ) lev = DebugLevel::Debug;
	if( Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ) lev = DebugLevel::Log;
	if( Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ) lev = DebugLevel::Warning;
	if( Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ) lev = DebugLevel::Error;

	OutputDebug( lev, "%s\n", CallbackData->pMessage );
	VK_ASSERT( !( Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ) );
	return VK_FALSE;
}

bool GameRenderHandler::OnCreate(GLFWwindow* window)
{
	ASSERT_LOG_RETURN(glfwVulkanSupported(), "Vulkan not supported");

	CHECK_LOG_RETURN( volkInitialize(), "Could not initialize volk" );//volkInitializeCustom(glfwGetInstanceProcAddress);

	{
		std::vector<const char *> instanceLayers;
		std::vector<const char *> instanceExtensions;

		{
			const char *wantedLayers[]{
				"VK_LAYER_KHRONOS_synchronization2",
				"VK_LAYER_KHRONOS_validation",
			};

			std::vector<VkLayerProperties> layerProps;
			CHECK_LOG_RETURN( vulkan_helpers::get_vector( layerProps, vkEnumerateInstanceLayerProperties ), "Cannot enumerate layers" );

			for( const char *layer : wantedLayers )
				for( const auto &prop : layerProps )
					if( strcmp( layer, prop.layerName ) == 0 )
					{
						instanceLayers.push_back( layer );
						break;
					}

			const char *wantedExts[]{
				VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
				VK_EXT_DEBUG_UTILS_EXTENSION_NAME
			};

			std::vector<VkExtensionProperties> instanceExts;
			CHECK_LOG_RETURN( vulkan_helpers::enumerate_instance_extensions( instanceExts, instanceLayers ), "Cannot enumerate instance extensions" );

			for( const char *ext : wantedExts )
				if( vulkan_helpers::is_extension_present( instanceExts, ext ) )
					instanceExtensions.push_back( ext );
		}

		const VkDebugUtilsMessengerCreateInfoEXT messengerCI = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext = nullptr,
			.flags = 0,
			.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				0,
			.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
				0,
			.pfnUserCallback = VulkanDebugCallback,
			.pUserData = nullptr
		};

		bool debUtils = vulkan_helpers::is_extension_present( instanceExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME );

		const void *pCreateInstancePNext = debUtils ? &messengerCI : nullptr;

		CHECK_LOG_RETURN( CreateInstance(instanceLayers, instanceExtensions, nullptr, true, pCreateInstancePNext, &vk.instance), "Could not create vulkan instance");
		vk.enabledLayers = move(instanceLayers);
		vk.enabledExts = move(instanceExtensions);

		if( debUtils )
			CHECK_LOG_RETURN( vkCreateDebugUtilsMessengerEXT( vk.instance, &messengerCI, nullptr, &vk.messenger ), "Could not setup debug callbacks");
		else
			vk.messenger = VK_NULL_HANDLE;
		
		CHECK_LOG_RETURN( glfwCreateWindowSurface( vk.instance, window, nullptr, &vk.surface ), "Could not create surface" );
	}

	glfwGetFramebufferSize( window, (int*)&kScreenWidth, (int*)&kScreenHeight );
	
	std::vector<VkPhysicalDevice> devices;
	CHECK_LOG_RETURN( vulkan_helpers::get_vector( devices, vkEnumeratePhysicalDevices, vk.instance ), "Could not enumerate physical devices" );

	VkPhysicalDevice best = VK_NULL_HANDLE;
	{
		VkPhysicalDeviceVulkan12Properties best12Props;
		VkPhysicalDeviceVulkan11Properties best11Props;
		VkPhysicalDeviceProperties2 bestProps;

		VkPhysicalDeviceVulkan12Features best12Feats;
		VkPhysicalDeviceVulkan11Features best11Feats;
		VkPhysicalDeviceFeatures2 bestFeats;

		auto GetTypeCost = []( VkPhysicalDeviceType type ) -> uint32_t
		{
			const VkPhysicalDeviceType order[] = {
				VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
				VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
				VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
				VK_PHYSICAL_DEVICE_TYPE_CPU,
				VK_PHYSICAL_DEVICE_TYPE_OTHER,
			};

			auto idx = find( begin(order), end(order), type );

			return (uint32_t)( end(order) - idx );
		};

		auto StripPatch = []( uint32_t ver ) -> uint32_t { 
			return VK_MAKE_API_VERSION( 0, VK_API_VERSION_MAJOR(ver), VK_API_VERSION_MINOR(ver), 0 );
		};

		for( VkPhysicalDevice dev : devices )
		{
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties( dev, &props );

			if( VK_API_VERSION_VARIANT( props.apiVersion ) == 0 &&
				props.apiVersion >= VK_API_VERSION_1_2 )
			{
				VkPhysicalDeviceVulkan12Properties cur12Props{};
				cur12Props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;

				VkPhysicalDeviceVulkan11Properties cur11Props{};
				cur11Props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
				cur11Props.pNext = &cur12Props;

				VkPhysicalDeviceProperties2 curProps{};
				curProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				curProps.pNext = &cur11Props;

				VkPhysicalDeviceVulkan12Features cur12Feats{};
				cur12Feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

				VkPhysicalDeviceVulkan11Features cur11Feats{};
				cur11Feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
				cur11Feats.pNext = &cur12Feats;

				VkPhysicalDeviceFeatures2 curFeats{};
				curFeats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				curFeats.pNext = &cur11Feats;

				vkGetPhysicalDeviceProperties2( dev, &curProps );
				vkGetPhysicalDeviceFeatures2( dev, &curFeats );

				auto selectNew = [&]() {
					best = dev; 
					bestProps = curProps;
					best11Props = cur11Props;
					best12Props = cur12Props;

					bestFeats = curFeats;
					best11Feats = cur11Feats;
					best12Feats = cur12Feats;
				};

				if( !best ) { selectNew(); continue; }

				uint32_t newDev = GetTypeCost( curProps.properties.deviceType );
				uint32_t oldDev = GetTypeCost( bestProps.properties.deviceType );

				if( oldDev != newDev ) { if( oldDev < newDev ) selectNew(); continue; }

				uint32_t newDevVer = StripPatch( curProps.properties.apiVersion );
				uint32_t oldDevVer = StripPatch( bestProps.properties.apiVersion );

				if( oldDevVer != newDevVer ) { if( oldDevVer < newDevVer ) selectNew(); continue; }
			}
		}

		TRACE(DebugLevel::Log, "Selected %s\n", bestProps.properties.deviceName );
	}

	VulkanQueue graphicsQueue;
	graphicsQueue.family = FindQueueFamilies( best, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT );

	std::vector<float> priorities{ 1.0f };
	std::vector<VkDeviceQueueCreateInfo> queueCI{
		VkDeviceQueueCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueFamilyIndex = graphicsQueue.family,
			.queueCount = 1,
			.pQueuePriorities = priorities.data()
		}
	};

	std::vector<VkExtensionProperties> devExtensions; 
	CHECK_LOG_RETURN( vulkan_helpers::enumerate_device_extensions( devExtensions, best, vk.enabledLayers ), "Could not enumerate extensions" );
	
	std::vector<const char *> deviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	void *lastFeatures = nullptr;

	if( vulkan_helpers::is_extension_present( devExtensions, VK_EXT_VALIDATION_CACHE_EXTENSION_NAME ) )
		deviceExtensions.push_back( VK_EXT_VALIDATION_CACHE_EXTENSION_NAME );

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
	VkPhysicalDevicePortabilitySubsetFeaturesKHR phPortSubset{};
	phPortSubset.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
	phPortSubset.pNext = lastFeatures;
	phPortSubset.events = VK_TRUE;

	if( vulkan_helpers::is_extension_present( devExtensions, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME ) )
	{
		lastFeatures = &phPortSubset;
		deviceExtensions.push_back( VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME );
	}
#endif // VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME

	VkPhysicalDeviceSynchronization2FeaturesKHR phSynchronization2{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
		.pNext = lastFeatures,
		.synchronization2 = VK_TRUE
	};

	lastFeatures = &phSynchronization2;
	deviceExtensions.push_back( VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME );

	VkPhysicalDeviceDynamicRenderingFeaturesKHR phRenderingFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
		.pNext = lastFeatures,
		.dynamicRendering = VK_TRUE
	};

	lastFeatures = &phRenderingFeatures;
	deviceExtensions.push_back( VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME );

	VkPhysicalDeviceVulkan12Features ph12Features{};
	ph12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	ph12Features.pNext = lastFeatures;
	ph12Features.bufferDeviceAddress = VK_TRUE;

	lastFeatures = &ph12Features;
	
	VkPhysicalDeviceVulkan11Features ph11Features{};
	ph11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	ph11Features.pNext = lastFeatures;

	lastFeatures = &ph11Features;

	VkPhysicalDeviceFeatures2 phFeatures{};
	phFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
	phFeatures.pNext = lastFeatures,
	phFeatures.features.samplerAnisotropy = VK_TRUE;
	phFeatures.features.shaderFloat64 = VK_TRUE;

	TRACE(DebugLevel::Log, "Before device creation\n");
	CHECK_LOG_RETURN( InitVulkanDevice( vk, best, queueCI, deviceExtensions, &phFeatures, vkDev ), "Could not create device" );

	TRACE(DebugLevel::Log, "Before render device creation\n");
	vkGetDeviceQueue( vkDev.device, graphicsQueue.family, 0, &graphicsQueue.queue );
	CHECK_LOG_RETURN( InitVulkanRenderDevice( vk, vkDev, graphicsQueue, kScreenWidth, kScreenHeight, vkRDev ), "Could not create reander device" );

	TRACE(DebugLevel::Log, "Before pipeline layout creation\n");
	CHECK_LOG_RETURN( CreateEngineDescriptorSetLayout( vkDev.device, &vkState.descriptorSetLayout ), "Could not create descriptor set layout" );
	CHECK_LOG_RETURN( CreatePipelineLayout( vkDev.device, 1, &vkState.descriptorSetLayout, 0, nullptr, &vkState.layout ), 
		"Could not create pipeline layout" );

	VkFormat depthFormat = FindDepthFormat( vkDev.physicalDevice );

	TRACE(DebugLevel::Log, "Before shader creation\n");
	{
		VkResult res = VK_SUCCESS;

		VkShaderModule vertShader = VK_NULL_HANDLE, fragShader = VK_NULL_HANDLE;
		if( res >= 0 ) res = CreateShaderModule( vkDev.device, "Shaders/Simple.vert", 0, nullptr, &vertShader );
		if( res >= 0 ) res = CreateShaderModule( vkDev.device, "Shaders/Simple.frag", 0, nullptr, &fragShader );
		if( res >= 0 )
		{
			TRACE( DebugLevel::Log, "Before pipeline creation\n" );
			VkGraphicsPipelineCreateInfo gpci{};
			FillGraphicsPipelineDefaults( &gpci );

			std::vector<VkPipelineShaderStageCreateInfo> shaders{
				FillShaderStage( VK_SHADER_STAGE_VERTEX_BIT, vertShader, "main" ),
				FillShaderStage( VK_SHADER_STAGE_FRAGMENT_BIT, fragShader, "main" ),
			};

			VkDynamicState dyncamicStates[] = {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR
			};
			VkPipelineDynamicStateCreateInfo dsci{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.dynamicStateCount = (uint32_t)size( dyncamicStates ),
				.pDynamicStates = data( dyncamicStates )
			};

			VkFormat colorFromats[] = {
				VK_FORMAT_B8G8R8A8_UNORM
			};

			VkPipelineRenderingCreateInfo prci{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.pNext = nullptr,
				.viewMask = 0,
				.colorAttachmentCount = (uint32_t)size( colorFromats ),
				.pColorAttachmentFormats = data( colorFromats ),
				.depthAttachmentFormat = depthFormat,
				.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
			};

			gpci.stageCount = (uint32_t)size( shaders );
			gpci.pStages = data( shaders );
			gpci.pDynamicState = &dsci;
			gpci.layout = vkState.layout;

			gpci.pNext = &prci;

			TRACE( DebugLevel::Log, "Before vkCreateGraphicsPipelines\n" );
			res = vkCreateGraphicsPipelines( vkDev.device, vkDev.pipelineCache, 1, &gpci, nullptr, &vkState.graphicsPipeline );
		}
		if( vertShader ) vkDestroyShaderModule( vkDev.device, vertShader, nullptr );
		if( fragShader ) vkDestroyShaderModule( vkDev.device, fragShader, nullptr );

		CHECK_LOG_RETURN( res, "Couldn't create pipeline" );
	}

	CHECK_LOG_RETURN( CreateDescriptorSetHelper( vkDev.device, 
		0, 
		(uint32_t)vkRDev.swapchainImages.size(),
		10, 10, 10, 
		&vkState.descriptorPool ), "Could not create descriptor pool" );

	const VkPhysicalDeviceProperties *props;
	vmaGetPhysicalDeviceProperties( vkDev.allocator, &props );

	VkDeviceSize uniformSize = 0;
	GetSuballocatedBufferSize( 
		std::vector<VkDeviceSize>(vkRDev.swapchainImages.size(), sizeof(glm::mat4) ),
		props->limits.minUniformBufferOffsetAlignment,
		&uniformSize,
		vkState.uniformBuffers
	);

	CHECK_LOG_RETURN( CreateBuffer( vkDev.allocator, uniformSize, 0,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
		&vkState.uniformBufferMemory.buffer, &vkState.uniformBufferMemory.bufferAllocation,
		nullptr ), "Could not create unifrom buffers" );
	
	CHECK_LOG_RETURN( CreateSSBOVertexBuffer( vkRDev, 
		"data/3dModels/SpaceShuttle.obj", 
		&vkState.modelBuffer.buffer, &vkState.modelBuffer.bufferAllocation,
		&vkState.vertexBuffer, &vkState.indexBuffer ), "Could not crate model" );

	CHECK_LOG_RETURN( CreateTextureSampler( vkDev.device, VK_FILTER_LINEAR, 
		VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		props->limits.maxSamplerAnisotropy, &vkState.textureSampler ), 
		"Could not create sampler" );
	
	CHECK_LOG_RETURN( CreateTextureImage( vkRDev, "data/3dModels/SpaceShuttle_BaseColor.png", 
		&vkState.texture ), "Could not create texture" );

	CHECK_LOG_RETURN( CreateEngineDescriptorSets( vkRDev, vkState ), "Cannot create descriptor sets" );

	{
		VkResult res = CreateImageResource( vkDev,
			depthFormat, VK_IMAGE_TYPE_2D, { kScreenWidth, kScreenHeight, 1 },
			1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 0,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			0, VMA_MEMORY_USAGE_AUTO,
			&vkState.depthResource );

		if( res >= 0 )
		{
			VkCommandBuffer cmdBuffer;
			res = BeginSingleTimeCommands(
				vkDev.device, vkRDev.commandPool, &cmdBuffer
			);

			if( res >= 0 )
			{
				TransitionImageLayoutCmd(
					cmdBuffer, vkState.depthResource.image, FormatGetAspects( depthFormat ),
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
					VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
					VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
				);

				res = EndSingleTimeCommands( vkDev.device, vkRDev.commandPool, vkRDev.graphicsQueue.queue, cmdBuffer );
			}
		}

		CHECK_LOG_RETURN( res, "Could not create depth resource" );
	}

	// tmp
	{
		const VkDescriptorSetLayoutBinding binding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr
		};

		const VkDescriptorSetLayoutCreateInfo dslci{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = 1,
			.pBindings = &binding,
		};

		CHECK_LOG_RETURN( vkCreateDescriptorSetLayout( vkDev.device, &dslci, nullptr, &computeSetLayout ),
			"Cannot create compute set layout" );

		const VkPushConstantRange pcr{
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.offset = 0,
			.size = sizeof( computePushConstants ),
		};

		const VkPipelineLayoutCreateInfo plci{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &computeSetLayout,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pcr
		};
		CHECK_LOG_RETURN( vkCreatePipelineLayout( vkDev.device, &plci, nullptr, &computeLayout ), 
			"Cannot create compute layout" );

		VkShaderModule compModule = VK_NULL_HANDLE;
		CHECK_LOG_RETURN( CreateShaderModule( vkDev.device, "Shaders/Mandelbrot.comp", 0, nullptr, &compModule ),
			"Cannot create compute shader module" );

		const VkComputePipelineCreateInfo cpci{
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.stage = VK_SHADER_STAGE_COMPUTE_BIT,
				.module = compModule,
				.pName = "main",
				.pSpecializationInfo = nullptr
			},
			.layout = computeLayout,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = -1
		};

		VkResult res = vkCreateComputePipelines( vkDev.device,
			vkDev.pipelineCache, 1, &cpci, nullptr, &compute );

		vkDestroyShaderModule( vkDev.device, compModule, nullptr );
		CHECK_LOG_RETURN( res, "Cannot compile compute pipeline" );

		const VkDescriptorPoolSize dps{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 10,
		};

		CHECK_LOG_RETURN( CreateDescriptorPool( vkDev.device, nullptr, 0, 
			(uint32_t)vkRDev.swapchainImages.size(), 1, &dps, &computePool ),
			"Cannot create compute pool" );

		computeDescriptors.resize( vkRDev.swapchainImages.size() );
		std::vector<VkDescriptorSetLayout> layout( computeDescriptors.size(), computeSetLayout );
		const VkDescriptorSetAllocateInfo dsai{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = computePool,
			.descriptorSetCount = (uint32_t)computeDescriptors.size(),
			.pSetLayouts = layout.data()
		};

		CHECK_LOG_RETURN( vkAllocateDescriptorSets( vkDev.device, &dsai, computeDescriptors.data() ), 
			"Cannot allocate compute ds" );
		
		std::vector<VkWriteDescriptorSet> writes;
		std::vector<VkDescriptorImageInfo> imageInfos;
		for( uint32_t i = 0; i < (uint32_t)computeDescriptors.size(); i++ )
		{
			imageInfos.push_back( VkDescriptorImageInfo{
				.sampler = VK_NULL_HANDLE,
				.imageView = vkRDev.swapchainImageViews[i],
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL
			} );
		}

		TRACE( DebugLevel::PrintAlways, "%d %d\n", (int)imageInfos.size(), (int)computeDescriptors.size() );

		for( uint32_t i = 0; i < (uint32_t)computeDescriptors.size(); i++ )
		{
			writes.push_back( VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = computeDescriptors[ i ],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.pImageInfo = &imageInfos[i],
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr
			} );
		}
		
		vkUpdateDescriptorSets( vkDev.device, (uint32_t)writes.size(), writes.data(), 0, nullptr );
	}
	//

	//for( uint32_t i = 0; i < (uint32_t)vkRDev.swapchainImageViews.size(); i++ )
	//	CHECK_LOG_RETURN( FillCommandBuffers( i ), "Could not fill command buffer" );

	lt = glfwGetTime();

	return true;
}

void GameRenderHandler::OnDraw(const void*dat )
{
	static double filteredDT = 1;
	auto t = glfwGetTime();
	auto dt = t - lt;
	lt = t;
	constexpr double alpha = 0.1;
	filteredDT = filteredDT * (1-alpha) + dt * alpha;

	uint32_t imageIdx;
	CHECK_LOG_RETURN_NOVAL( vkAcquireNextImageKHR( vkDev.device, vkRDev.swapchain, UINT64_MAX,
		vkRDev.semophore, VK_NULL_HANDLE, &imageIdx ), "Cannot acquire image" );

	OutputDebug(DebugLevel::Log, "%2d: %8.4fms (%6.2f)\n", imageIdx, dt*1000, 1. / filteredDT );
	
	{
		void *BufferMemory;
		CHECK_LOG_RETURN_NOVAL( vmaMapMemory( vkDev.allocator, 
			vkState.uniformBufferMemory.bufferAllocation, &BufferMemory ), "could not map uniform buffer" );

		{
			
			void *memory = (char*)BufferMemory + vkState.uniformBuffers[ imageIdx ].offset;

			const glm::mat4 reverseDepthMatrrix(
				1.f, 0.f, 0.f, 0.f,
				0.f, 1.f, 0.f, 0.f,
				0.f, 0.f,-1.f, 0.f,
				0.f, 0.f, 1.f, 1.f
			);
			glm::mat4 persp = reverseDepthMatrrix * glm::infinitePerspective( glm::pi<float>() * .5f, ( (float)kScreenWidth / kScreenHeight ), 1.f );
			glm::mat4 view = glm::lookAt( glm::vec3{100.f, 100.f, 0.f }, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f} );
			glm::mat4 model = 
			glm::rotate( 
				glm::scale( glm::mat4(1.0f), glm::vec3(0.8f) ),
				(float)glfwGetTime() * glm::pi<float>() * 2.f, glm::vec3( 0.0f, 1.0f, 0.0f ) );

			glm::mat4 mvp = persp * view * model;

			*( (glm::mat4*)memory ) = mvp;
		}

		vmaUnmapMemory( vkDev.allocator, vkState.uniformBufferMemory.bufferAllocation );
		CHECK_LOG_RETURN_NOVAL( vmaFlushAllocation( vkDev.allocator, vkState.uniformBufferMemory.bufferAllocation, 
			vkState.uniformBuffers[imageIdx].offset, vkState.uniformBuffers[imageIdx].size ),
			"Could not flush unifrom buffers" );
	}

	pPC = (const computePushConstants *)dat;

	CHECK_LOG_RETURN_NOVAL( FillCommandBuffers( imageIdx ), "Cannot fill cmd buffer" );

	VkPipelineStageFlags waitFalgs[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	VkSubmitInfo si{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vkRDev.semophore,
		.pWaitDstStageMask = data(waitFalgs),
		.commandBufferCount = 1,
		.pCommandBuffers = &vkRDev.commandBuffers[ imageIdx ],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &vkRDev.renderSemaphore,
	};
	CHECK_LOG_RETURN_NOVAL( vkQueueSubmit( vkRDev.graphicsQueue.queue, 1, &si, vkRDev.fence ), "Cannot enqueue cmdBuffers" );

	VkResult res;
	VkPresentInfoKHR pi{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vkRDev.renderSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &vkRDev.swapchain,
		.pImageIndices = &imageIdx,
		.pResults = &res,
	};

	CHECK_LOG_RETURN_NOVAL( vkQueuePresentKHR( vkRDev.graphicsQueue.queue, &pi ), "Cannot enqueue present" );
	CHECK_LOG_RETURN_NOVAL( vkWaitForFences( vkDev.device, 1, &vkRDev.fence, VK_TRUE, UINT64_MAX ), "Cannot wait for fence" );
	CHECK_LOG_RETURN_NOVAL( vkResetFences( vkDev.device, 1, &vkRDev.fence ), "Cannot reset fence" );
	CHECK_LOG_RETURN_NOVAL( res, "Cannot present" );
}

void GameRenderHandler::OnDestroy()
{
	
}

GameRenderHandler::~GameRenderHandler()
{
	if( vkDev.device )
	{
		vkDeviceWaitIdle( vkDev.device );
		vkDestroyDescriptorSetLayout( vkDev.device, computeSetLayout, nullptr );
		vkDestroyPipelineLayout( vkDev.device, computeLayout, nullptr );
		vkDestroyPipeline( vkDev.device, compute, nullptr );
		vkDestroyDescriptorPool( vkDev.device, computePool, nullptr );

		DestroyVulkanState( vkDev, vkState );
		DestroyVulkanRendererDevice( vkRDev );
	}
	DestroyVulkanDevice( vkDev );
	DestroyVulkanInstance( vk );
}

VkResult GameRenderHandler::FillCommandBuffers( uint32_t index )
{
	const VkCommandBufferBeginInfo bi = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr
	};

	VkCommandBuffer CmdBuffer = vkRDev.commandBuffers[ index ];
	VK_CHECK_RET( vkBeginCommandBuffer( CmdBuffer, &bi ) );
	
	TransitionImageLayoutCmd( CmdBuffer, vkRDev.swapchainImages[ index ], VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, //VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT );
		//VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT );
	/*
	VkRenderingAttachmentInfo colorAttachment = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.pNext = nullptr,
		.imageView = vkRDev.swapchainImageViews[ index ],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.resolveMode = VK_RESOLVE_MODE_NONE,
		.resolveImageView = VK_NULL_HANDLE,
		.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { .color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } } },
	};

	VkRenderingAttachmentInfo depthAttachment = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.pNext = nullptr,
		.imageView = vkState.depthResource.imageView,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.resolveMode = VK_RESOLVE_MODE_NONE,
		.resolveImageView = VK_NULL_HANDLE,
		.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.clearValue = { .depthStencil = { .depth = 0.0f, .stencil = 0 } },
	};

	VkRenderingInfo rinfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.pNext = nullptr,
		.flags = 0,
		.renderArea = { {0, 0}, {kScreenWidth, kScreenHeight} },
		.layerCount = 1,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachment,
		.pDepthAttachment = &depthAttachment,
		.pStencilAttachment = nullptr
	};
	
	vkCmdBeginRenderingKHR( CmdBuffer, &rinfo );

		VkViewport vp{ 
			.x = 0, .y = 0,
			.width = (float)kScreenWidth, .height = (float)kScreenHeight,	
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};
		vkCmdSetViewport( CmdBuffer, 0, 1, &vp );

		VkRect2D sc{
			.offset = {0,0},
			.extent = {kScreenWidth, kScreenHeight}
		};
		vkCmdSetScissor( CmdBuffer, 0, 1, &sc );

		uint32_t offset = (uint32_t)vkState.uniformBuffers[index].offset;
		vkCmdBindPipeline( CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkState.graphicsPipeline );
		vkCmdBindDescriptorSets( CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vkState.layout, 0, 1, &vkState.descriptorSet, 1, &offset );//&vkState.descriptorSets[ index ], 0, nullptr );
		
		vkCmdBindIndexBuffer( CmdBuffer, vkState.modelBuffer.buffer, vkState.indexBuffer.offset, VK_INDEX_TYPE_UINT32 );
		vkCmdDrawIndexed( CmdBuffer, (uint32_t)( vkState.indexBuffer.size / sizeof( uint32_t ) ), 1, 0, 0, 0 );

	vkCmdEndRenderingKHR( CmdBuffer );
	*/

	vkCmdBindPipeline( CmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute );
	vkCmdBindDescriptorSets( CmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 1,
		&computeDescriptors[ index ], 0, nullptr );

	//computePushConstants pc{
	//	.start = glm::dvec2( -2, -2 ),
	//	.increment = glm::dvec2( 4, 4 ) / glm::dvec2( kScreenWidth, kScreenHeight ) 
	//};
	vkCmdPushConstants( CmdBuffer, computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 
		0, sizeof(*pPC), pPC );
	const int blockSize = 8;
	vkCmdDispatch( CmdBuffer, AlignUp(kScreenWidth, blockSize) / blockSize, AlignUp(kScreenHeight, blockSize) / blockSize, 1 );

	TransitionImageLayoutCmd( CmdBuffer, vkRDev.swapchainImages[ index ], VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL,//VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
		//VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0 );
	
	return vkEndCommandBuffer( CmdBuffer );
}