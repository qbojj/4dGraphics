#include "GameRenderHandler.h"
#include "GameTickHandler.h"

#include "Debug.h"
#include "VulkanHelpers.h"

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

	DebugLevel lev = DebugLevel::Debug;
	const char *sev = "unknown";
	if( Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT ) lev = DebugLevel::Debug, sev = "verbose log";
	if( Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ) lev = DebugLevel::Log, sev = "info";
	if( Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ) lev = DebugLevel::Warning, sev = "warning";
	if( Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ) lev = DebugLevel::Error, sev = "error";

	std::string type;
	if( Type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT ) type += "general ";
	if( Type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ) type += "validation ";
	if( Type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT ) type += "performance ";

	OutputDebug( lev, "%s%s: %s\n", type.c_str(), sev, CallbackData->pMessage );
	VK_ASSERT( !( Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ) );
	return VK_FALSE;
}

bool GameRenderHandler::OnCreate(GLFWwindow* window)
{
	VkResult res = VK_SUCCESS;
	ASSERT_LOG_RETURN(glfwVulkanSupported(), "Vulkan not supported");

	volkInitializeCustom(glfwGetInstanceProcAddress);

	std::vector<const char *> instanceLayers{
		"VK_LAYER_KHRONOS_validation"
	};

	std::vector<const char *> instanceExtensions{
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	};

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

	CHECK_LOG_RETURN( CreateInstance(instanceLayers, instanceExtensions, nullptr, false, &messengerCI, &vk.instance), "Could not create vulkan instance");

	CHECK_LOG_RETURN( vkCreateDebugUtilsMessengerEXT( vk.instance, &messengerCI, nullptr, &vk.messenger ), "Could not setup debug callbacks");

	CHECK_LOG_RETURN( glfwCreateWindowSurface( vk.instance, window, nullptr, &vk.surface ), "Could not create surface" );
	
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

	VkPhysicalDeviceDynamicRenderingFeaturesKHR phRenderingFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
		.pNext = nullptr,
		.dynamicRendering = VK_TRUE
	};

	VkPhysicalDeviceVulkan12Features ph12Features{};
	ph12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	ph12Features.pNext = &phRenderingFeatures;
	ph12Features.bufferDeviceAddress = VK_TRUE;
	ph12Features.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;

	VkPhysicalDeviceVulkan11Features ph11Features{};
	ph11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	ph11Features.pNext = &ph12Features;

	VkPhysicalDeviceFeatures2 phFeatures{};
	phFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
	phFeatures.pNext = &ph11Features,
	phFeatures.features.samplerAnisotropy = VK_TRUE;

	std::vector<const char *> deviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
	};

	TRACE(DebugLevel::Log, "Before device creation\n");
	CHECK_LOG_RETURN( InitVulkanDevice( vk, best, queueCI, deviceExtensions, &phFeatures, vkDev ), "Could not create device" );

	TRACE(DebugLevel::Log, "Before render device creation\n");
	vkGetDeviceQueue( vkDev.device, graphicsQueue.family, 0, &graphicsQueue.queue );
	CHECK_LOG_RETURN( InitVulkanRenderDevice( vk, vkDev, graphicsQueue, kScreenWidth, kScreenHeight, vkRDev ), "Could not create reander device" );

	TRACE(DebugLevel::Log, "Before pipeline layout creation\n");
	CHECK_LOG_RETURN( CreateEngineDescriptorSetLayout( vkDev.device, &vkState.descriptorSetLayout ), "Could not create descriptor set layout" );
	CHECK_LOG_RETURN( CreatePipelineLayout( vkDev.device, 1, &vkState.descriptorSetLayout, 0, nullptr, &vkState.layout ), 
		"Could not create pipeline layout" );

	TRACE(DebugLevel::Log, "Before shader creation\n");
	res = VK_SUCCESS;
	VkShaderModule vertShader = VK_NULL_HANDLE, fragShader = VK_NULL_HANDLE;
	if( res >= 0 ) res = CreateShaderModule( vkDev.device, "Shaders/Simple.vert", 0, nullptr, &vertShader );
	if( res >= 0 ) res = CreateShaderModule( vkDev.device, "Shaders/Simple.frag", 0, nullptr, &fragShader );
	if( res >= 0 ) 
	{
		TRACE(DebugLevel::Log, "Before pipeline creation\n");
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
			.dynamicStateCount = (uint32_t)size(dyncamicStates),
			.pDynamicStates = data(dyncamicStates)
		};

		VkFormat colorFromats[] = {
			VK_FORMAT_B8G8R8A8_UNORM
		};

		VkPipelineRenderingCreateInfo prci{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.pNext = nullptr,
			.viewMask = 0,
			.colorAttachmentCount = (uint32_t)size(colorFromats),
			.pColorAttachmentFormats = data(colorFromats),
			.depthAttachmentFormat = VK_FORMAT_D24_UNORM_S8_UINT, // TODO:
			.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
		};
		
		gpci.stageCount = (uint32_t)size(shaders);
		gpci.pStages = data(shaders);
		gpci.pDynamicState = &dsci;
		gpci.layout = vkState.layout;

		gpci.pNext = &prci;

		TRACE(DebugLevel::Log, "Before vkCreateGraphicsPipelines\n");
		res = vkCreateGraphicsPipelines( vkDev.device, vkDev.pipelineCache, 1, &gpci, nullptr, &vkState.graphicsPipeline );
	}
	if( vertShader ) vkDestroyShaderModule( vkDev.device, vertShader, nullptr );
	if( fragShader ) vkDestroyShaderModule( vkDev.device, fragShader, nullptr );

	CHECK_LOG_RETURN( res, "Couldn't create pipeline" );

	CHECK_LOG_RETURN( CreateDescriptorSetHelper( vkDev.device, 
		VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, 
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

	CHECK_LOG_RETURN( CreateImageView( vkDev.device, vkState.texture.image, 
		VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, &vkState.texture.imageView ),
		"Could not create image view" );

	CHECK_LOG_RETURN( CreateEngineDescriptorSets( vkRDev, vkState ), "Cannot create descriptor sets" );

	CHECK_LOG_RETURN( CreateDepthResource( vkRDev, kScreenWidth, kScreenHeight, &vkState.depthResource ),
		"Could not create depth resource" );

	for( uint32_t i = 0; i < (uint32_t)vkRDev.swapchainImageViews.size(); i++ )
		CHECK_LOG_RETURN( FillCommandBuffers( i ), "Could not fill command buffer" );

	lt = glfwGetTime();

	return true;
}

void GameRenderHandler::OnDraw(const void* )
{
	static double filteredDT = 0;
	auto t = glfwGetTime();
	auto dt = t - lt;
	lt = t;
	constexpr double alpha = 0.01;
	filteredDT = filteredDT * (1-alpha) + dt * alpha;

	VkResult res;
	uint32_t imageIdx;
	CHECK_LOG_RETURN_NOVAL( res = vkAcquireNextImageKHR( vkDev.device, vkRDev.swapchain, UINT64_MAX,
		vkRDev.semophore, VK_NULL_HANDLE, &imageIdx ), "Cannot acquire image" );

	//OutputDebug(DebugLevel::Log, "%2d: %8.4fms (%6.2f)\n", imageIdx, dt*1000, 1. / filteredDT );
	
	//CHECK_LOG_RETURN_NOVAL( vkQueueWaitIdle( vkRDev.graphicsQueue.queue ), "Could not wait for idle" );

	{
		void *BufferMemory;
		CHECK_LOG_RETURN_NOVAL( vmaMapMemory( vkDev.allocator, 
			vkState.uniformBufferMemory.bufferAllocation, &BufferMemory ), "could not map uniform buffer" );


		//for( uint32_t imageIdx = 0; imageIdx < (uint32_t)vkRDev.swapchainImages.size(); imageIdx++ )
		{
			
			void *memory = (char*)BufferMemory + vkState.uniformBuffers[ imageIdx ].offset;

			const glm::mat4 reverseDepthMatrrix(
				1.f, 0.f, 0.f, 0.f,
				0.f, 1.f, 0.f, 0.f,
				0.f, 0.f,-1.f, 0.f,
				0.f, 0.f, 1.f, 1.f
			);
			glm::mat4 persp = reverseDepthMatrrix * glm::infinitePerspective( glm::pi<float>() * .5f, ( (float)kScreenWidth / kScreenHeight ), 1.f );
			glm::mat4 view = glm::lookAt( glm::vec3(100.f, 100.f, 0.f ), {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f} );
			glm::mat4 model = 
			glm::rotate( 
				glm::scale( glm::identity<glm::mat4>(), glm::vec3(0.8f) ),
				(float)glfwGetTime() / 60.f * glm::pi<float>() * 2.f, glm::vec3( 0.0f, 1.0f, 0.0f ) );

			glm::mat4 mvp = persp * view * model;

			*( (glm::mat4*)memory ) = mvp;

		}

		vmaUnmapMemory( vkDev.allocator, vkState.uniformBufferMemory.bufferAllocation );
		CHECK_LOG_RETURN_NOVAL( vmaFlushAllocation( vkDev.allocator, vkState.uniformBufferMemory.bufferAllocation, 0, VK_WHOLE_SIZE ),
			"Could not flush unifrom buffers" );
	}

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
	CHECK_LOG_RETURN_NOVAL( res = vkQueueSubmit( vkRDev.graphicsQueue.queue, 1, &si, VK_NULL_HANDLE), "Cannot enqueue cmdBuffers" );

	VkPresentInfoKHR pi{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vkRDev.renderSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &vkRDev.swapchain,
		.pImageIndices = &imageIdx,
		.pResults = nullptr,
	};

	CHECK_LOG_RETURN_NOVAL( res = vkQueuePresentKHR( vkRDev.graphicsQueue.queue, &pi ), "Cannot enqueue present" );
}

void GameRenderHandler::OnDestroy()
{
	
}

GameRenderHandler::~GameRenderHandler()
{
	if( vkDev.device )
	{
		vkDeviceWaitIdle( vkDev.device );
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
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // TODO:
		.pInheritanceInfo = nullptr
	};

	VkCommandBuffer CmdBuffer = vkRDev.commandBuffers[ index ];
	VK_CHECK_RET( vkBeginCommandBuffer( CmdBuffer, &bi ) );

	TransitionImageLayoutCmd( CmdBuffer, vkRDev.swapchainImages[ index ], VK_FORMAT_B8G8R8A8_UNORM,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT );
	
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

		vkCmdBindPipeline( CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkState.graphicsPipeline );
		vkCmdBindDescriptorSets( CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vkState.layout, 0, 1, &vkState.descriptorSets[ index ], 0, nullptr );
		vkCmdDraw( CmdBuffer, (uint32_t)( vkState.indexBuffer.size / sizeof( uint32_t ) ), 1, 0, 0 );

	vkCmdEndRenderingKHR( CmdBuffer );

	TransitionImageLayoutCmd( CmdBuffer, vkRDev.swapchainImages[ index ], VK_FORMAT_B8G8R8A8_UNORM,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0 );
	
	return vkEndCommandBuffer( CmdBuffer );
}