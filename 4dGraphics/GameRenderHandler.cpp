#include "stdafx.h"
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
			logmsg " (cause: %s)" __VA_OPT__(, ) __VA_ARGS__,	\
			VulkanResultErrorCause(result_check_log_));			\
		return false;								\
	}												\
}while(0)

#if IS_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
	VkDebugUtilsMessageTypeFlagsEXT Type,
	const VkDebugUtilsMessengerCallbackDataEXT * CallbackData,
	void *UserData )
{
	(void)Severity;
	(void)Type;
	(void)CallbackData;
	(void)UserData;

	OutputDebug( DebugLevel::PrintAlways, "Layer %s", CallbackData->pMessage );
	return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanDebugReportCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objectType,
	uint64_t object, size_t location, int32_t messageCode,
	const char *pLayerPrefix,
	const char *pMessage,
	void *UserData )
{
	(void)flags;
	(void)objectType;
	(void)object;
	(void)location;
	(void)messageCode;
	(void)pLayerPrefix;
	(void)pMessage;
	(void)UserData;

	DebugLevel lv = DebugLevel::PrintAlways;
	if( flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT ) lv = DebugLevel::Debug;
	if( flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT ) lv = DebugLevel::Log;
	if( flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT ) lv = DebugLevel::Log;
	if( flags & VK_DEBUG_REPORT_WARNING_BIT_EXT ) lv = DebugLevel::Warning;
	if( flags & VK_DEBUG_REPORT_ERROR_BIT_EXT ) lv = DebugLevel::Error;

	OutputDebug( lv, "Debug callback (%s): %s\n", pLayerPrefix, pMessage );
	return VK_FALSE;
}
#endif

bool GameRenderHandler::OnCreate(GLFWwindow* window)
{
	ASSERT_LOG_RETURN(glfwVulkanSupported(), "Vulkan not supported");

	volkInitializeCustom(glfwGetInstanceProcAddress);
	return true;
	CHECK_LOG_RETURN(CreateInstance(NULL, &vk.instance), "Could not create vulkan instance");

#if IS_DEBUG
	CHECK_LOG_RETURN(SetupDebugCallbacks( vk.instance,
		&vk.messenger, VulkanDebugCallback, nullptr,
		&vk.reportCallback, VulkanDebugReportCallback, nullptr ), "Could not setup debug callbacks");
#endif

	CHECK_LOG_RETURN( glfwCreateWindowSurface( vk.instance, window, nullptr, &vk.surface ), "Could not create surface" );
	/*
	int width, height;
	glfwGetFramebufferSize( window, &width, &height );
	VkPhysicalDeviceFeatures2 phFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = nullptr,
		.features = {
		}
	};
	CHECK_LOG_RETURN( InitVulkanRenderDevice( vk, vkDev, width, height, [&]( auto ) { return true; }, &phFeatures ), "Could not create device" );
	*/
	return true;
}

void GameRenderHandler::OnDraw(const void* )
{
}

void GameRenderHandler::OnDestroy()
{
	
}

GameRenderHandler::~GameRenderHandler()
{
	//DestroyVulkanRendererDevice( vkDev );
	//DestroyVulkanInstance( vk );
}

VkResult fillCommandBuffers( VulkanRenderDevice &/* vkDev */, size_t /* index */ )
{
	/*
	const VkCommandBufferBeginInfo bi = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
		.pInheritanceInfo = nullptr
	};

	const VkClearColorValue clearValueColor = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } };

	const VkClearValue clearValues[] = {
		VkClearValue{.color = clearValueColor },
		VkClearValue{.depthStencil = {1.0f, 0 } }
	};

	const VkRect2D screenRect = {
		.offset = { 0, 0 },
		.extent = {	.width = kScreenWidth,
					.height = kScreenHeight }
	};

	VkCommandBuffer CmdBuffer = vkDev.commandBuffers[ index ];
	VK_CHECK_RET( vkBeginCommandBuffer( CmdBuffer, &bi ) );
	const VkRenderPassBeginInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,
		.renderPass = vkState.renderPass,
		.framebuffer = vkState.swapchainFramebuffers[ index ],
		.renderArea = screenRect,
		.clearValueCount = (uint32_t)size( clearValues ),
		.pClearValues = data( clearValues )
	};
	vkCmdBeginRenderPass( CmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );
	vkCmdBindPipeline( CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkState.graphicsPipeline );
	vkCmdBindDescriptorSets( CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vkState.pipelineLayout, 0, 1, vkState.descriptorSets[ index ], 0, nullptr );
	vkCmdDraw( CmdBuffer, (uint32_t)( indexBufferSize / sizeof( uint32_t ) ), 1, 0, 0 );
	vkCmdEndRenderPass( CmdBuffer );
	
	return vkEndCommandBuffer( CmdBuffer );
	*/
	return VK_SUCCESS;
}