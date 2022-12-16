#pragma once

#include "Shader.h"

#include <memory>
#include <volk.h>
#include "VulkanHelpers.h"

#include "Debug.h"
#include <exception>

class vulkan_error : public std::runtime_error
{
public:
	vulkan_error(VkResult err, const std::string &message) 
		: runtime_error( message + ": " + VulkanResultErrorCause(err) ), error(err) {};
	VkResult error;
};

struct SDL_Window;
class GameRenderHandler
{
public:
	GameRenderHandler(SDL_Window*);
	void OnDraw(const void* FData);
	~GameRenderHandler();
protected:

	VulkanInstance vk;
	VulkanDevice vkDev;
	VulkanRenderDevice vkRDev;

	VulkanState vkState;

	struct computePushConstants{
		glm::dvec2 start;
		glm::dvec2 increment;
	};

	const computePushConstants *pPC;

	VkDescriptorSetLayout computeSetLayout;
	VkPipelineLayout computeLayout;
	VkDescriptorPool computePool;
	std::vector<VkDescriptorSet> computeDescriptors;
	VkPipeline compute;

	double lt;
	VkResult FillCommandBuffers( uint32_t index );
	VkResult AdvanceFrame( uint32_t *imageIdx );
	VkResult EndFrame( uint32_t imageIdx );
	VkResult RecreateSwapchain();
	void ClearDestructionQueue( uint64_t until );
};