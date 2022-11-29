#pragma once

#include "GameCore.h"
#include "Shader.h"

#include "Collisions.h"
#include <memory>
#include <volk.h>
#include "VulkanHelpers.h"

class GameRenderHandler : public RenderHandler
{
public:
	bool OnCreate(GLFWwindow*) override;
	void OnDraw(const void* FData) override;
	void OnDestroy() override;
	~GameRenderHandler() override;
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

template<typename RH >
class GameRenderHandlerIndirect : public RenderHandler
{
public:
	bool OnCreate(GLFWwindow*wnd) override
	{ 
		renderer = std::make_unique<RH>(); 
		return renderer->OnCreate( wnd ); 
	}
	void OnDraw(const void* FData) { renderer->OnDraw(FData); }
	void OnDestroy() override { renderer->OnDestroy(); }
protected:
	std::unique_ptr<RH> renderer; // freed in destructor
};